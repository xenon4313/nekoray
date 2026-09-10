#include "SimpleRouteEditor.h"
#include "ProcessSelectDialog.h"

#include "db/Database.hpp"
#include "db/ConfigBuilder.hpp"
#include "main/NekoGui.hpp"
#include "main/NekoGui_Utils.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QAbstractItemView>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QFontDatabase>
#include <QComboBox>
#include <QMap>

namespace {
constexpr int kRoleMatcher = Qt::UserRole;
constexpr int kRoleProfileId = Qt::UserRole + 1;
constexpr int kRoleOutbound = Qt::UserRole + 2;
} // namespace

void SimpleRouteEditor::addListRow(QListWidget *list, const QString &text) {
    auto t = text.trimmed();
    if (t.isEmpty() || !list) return;

    // Normalize URL/scheme/path/port:
    if (t.contains("://")) {
        t = t.section("://", 1);
    }
    if (t.contains('/')) {
        t = t.section('/', 0, 0);
    }
    if (t.contains(':')) {
        t = t.section(':', 0, 0);
    }
    while (t.startsWith("*.")) t = t.mid(2);
    if (t.startsWith(".") && t.indexOf('.', 1) > 0) {
        t = t.mid(1);
    }
    t = t.trimmed().toLower();
    if (t.isEmpty()) return;

    for (int i = 0; i < list->count(); ++i) {
        if (list->item(i)->text().compare(t, Qt::CaseInsensitive) == 0) return;
    }
    list->addItem(t);
}

void SimpleRouteEditor::removeMatching(QListWidget *list, const QString &text) {
    if (!list) return;
    auto t = text.trimmed().toLower();
    if (t.contains("://")) t = t.section("://", 1);
    if (t.contains('/')) t = t.section('/', 0, 0);
    if (t.contains(':')) t = t.section(':', 0, 0);
    while (t.startsWith("*.")) t = t.mid(2);
    if (t.startsWith(".") && t.indexOf('.', 1) > 0) t = t.mid(1);
    t = t.trimmed().toLower();
    if (t.isEmpty()) return;

    for (int i = list->count() - 1; i >= 0; --i) {
        if (list->item(i)->text().compare(t, Qt::CaseInsensitive) == 0) {
            delete list->takeItem(i);
        }
    }
}


void SimpleRouteEditor::removeSelected(QListWidget *list) {
    if (!list) return;
    const auto items = list->selectedItems();
    for (auto *item: items) {
        delete list->takeItem(list->row(item));
    }
}

QJsonArray SimpleRouteEditor::collectList(QListWidget *list) {
    QJsonArray arr;
    if (!list) return arr;
    for (int i = 0; i < list->count(); ++i) arr += list->item(i)->text();
    return arr;
}

QString SimpleRouteEditor::profileOutboundTag(int profileId) {
    return NekoGui::ProfileOutboundTag(profileId);
}

int SimpleRouteEditor::profileIdFromOutboundTag(const QString &tag) {
    if (!tag.startsWith(QStringLiteral("p-"))) return -1;
    bool ok = false;
    const int id = tag.mid(2).toInt(&ok);
    return ok ? id : -1;
}

QString SimpleRouteEditor::displayNameForProfile(int profileId) const {
    auto pf = NekoGui::profileManager->GetProfile(profileId);
    if (pf && pf->bean) return pf->bean->DisplayName();
    return QStringLiteral("#%1").arg(profileId);
}

void SimpleRouteEditor::refillServerCombo(QComboBox *box) {
    if (!box) return;
    const int prev = box->currentData().toInt();
    box->clear();
    auto group = NekoGui::profileManager->CurrentGroup();
    if (!group) return;
    for (const auto &pf: group->ProfilesWithOrder()) {
        if (!pf || !pf->bean) continue;
        box->addItem(pf->bean->DisplayName(), pf->id);
    }
    if (prev >= 0) {
        const int idx = box->findData(prev);
        if (idx >= 0) box->setCurrentIndex(idx);
    }
}

void SimpleRouteEditor::addServerMapRow(QListWidget *list, const QString &matcher, int profileId) {
    auto t = matcher.trimmed();
    if (t.isEmpty() || !list || profileId < 0) return;
    const QString outbound = profileOutboundTag(profileId);
    for (int i = 0; i < list->count(); ++i) {
        auto *it = list->item(i);
        if (it->data(kRoleMatcher).toString().compare(t, Qt::CaseInsensitive) == 0 &&
            it->data(kRoleProfileId).toInt() == profileId) {
            return;
        }
    }
    auto *item = new QListWidgetItem(
        QStringLiteral("%1  →  %2").arg(t, displayNameForProfile(profileId)), list);
    item->setData(kRoleMatcher, t);
    item->setData(kRoleProfileId, profileId);
    item->setData(kRoleOutbound, outbound);
    list->addItem(item);
}

SimpleRouteEditor::ListPage SimpleRouteEditor::makeDomainPage(QWidget *parent, const QString &placeholder, ListPage *opposing) {
    ListPage page;
    auto *lay = new QVBoxLayout(parent);
    page.list = new QListWidget(parent);
    page.list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    lay->addWidget(page.list, 1);
    auto *row = new QHBoxLayout;
    page.edit = new QLineEdit(parent);
    page.edit->setPlaceholderText(placeholder);
    auto *addBtn = new QPushButton(tr("Add"), parent);
    auto *delBtn = new QPushButton(tr("Remove"), parent);
    row->addWidget(page.edit, 1);
    row->addWidget(addBtn);
    row->addWidget(delBtn);
    lay->addLayout(row);
    auto onAdd = [=] {
        const auto text = page.edit->text();
        addListRow(page.list, text);
        if (opposing && opposing->list) {
            removeMatching(opposing->list, text);
        }
        page.edit->clear();
    };
    connect(addBtn, &QPushButton::clicked, this, onAdd);
    connect(delBtn, &QPushButton::clicked, this, [=] { removeSelected(page.list); });
    connect(page.edit, &QLineEdit::returnPressed, this, onAdd);
    return page;
}

SimpleRouteEditor::ListPage SimpleRouteEditor::makeAppPage(QWidget *parent, const QString &placeholder, ListPage *opposing) {
    ListPage page;
    auto *lay = new QVBoxLayout(parent);
    page.list = new QListWidget(parent);
    page.list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    lay->addWidget(page.list, 1);
    auto *row = new QHBoxLayout;
    page.edit = new QLineEdit(parent);
    page.edit->setPlaceholderText(placeholder);
    auto *addBtn = new QPushButton(tr("Add"), parent);
    auto *browseBtn = new QPushButton(tr("Browse…"), parent);
    auto *runningBtn = new QPushButton(tr("Running…"), parent);
    auto *delBtn = new QPushButton(tr("Remove"), parent);
    row->addWidget(page.edit, 1);
    row->addWidget(addBtn);
    row->addWidget(browseBtn);
    row->addWidget(runningBtn);
    row->addWidget(delBtn);
    lay->addLayout(row);
    auto addApp = [=](const QString &text) {
        addListRow(page.list, text);
        if (opposing && opposing->list) {
            removeMatching(opposing->list, text);
        }
    };
    connect(addBtn, &QPushButton::clicked, this, [=] {
        addApp(page.edit->text());
        page.edit->clear();
    });
    connect(delBtn, &QPushButton::clicked, this, [=] { removeSelected(page.list); });
    connect(page.edit, &QLineEdit::returnPressed, this, [=] {
        addApp(page.edit->text());
        page.edit->clear();
    });
    connect(browseBtn, &QPushButton::clicked, this, [=] {
        auto path = QFileDialog::getOpenFileName(this, tr("Select application"), QString(),
#ifdef Q_OS_WIN
                                                 tr("Programs (*.exe);;All (*.*)")
#else
                                                 tr("All (*.*)")
#endif
        );
        if (path.isEmpty()) return;
        addApp(QFileInfo(path).fileName());
    });
    connect(runningBtn, &QPushButton::clicked, this, [=] {
        ProcessSelectDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            for (const auto &proc : dlg.selectedProcessNames()) {
                addApp(proc);
            }
        }
    });
    return page;
}

SimpleRouteEditor::ServerMapPage SimpleRouteEditor::makeServerDomainPage(QWidget *parent) {
    ServerMapPage page;
    auto *lay = new QVBoxLayout(parent);
    auto *hint = new QLabel(tr("Route specific sites through a chosen server "
                               "(even if another server is currently connected)."),
                            parent);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#666;");
    lay->addWidget(hint);

    page.list = new QListWidget(parent);
    page.list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    lay->addWidget(page.list, 1);

    auto *row = new QHBoxLayout;
    page.server = new QComboBox(parent);
    page.server->setMinimumWidth(140);
    refillServerCombo(page.server);
    page.edit = new QLineEdit(parent);
    page.edit->setPlaceholderText(tr("2ip.ru  or  .example.com"));
    auto *addBtn = new QPushButton(tr("Add"), parent);
    auto *delBtn = new QPushButton(tr("Remove"), parent);
    row->addWidget(page.server);
    row->addWidget(page.edit, 1);
    row->addWidget(addBtn);
    row->addWidget(delBtn);
    lay->addLayout(row);

    connect(addBtn, &QPushButton::clicked, this, [=] {
        if (page.server->currentIndex() < 0) {
            QMessageBox::warning(this, tr("Sites by server"), tr("Select a server first."));
            return;
        }
        addServerMapRow(page.list, page.edit->text(), page.server->currentData().toInt());
        page.edit->clear();
    });
    connect(delBtn, &QPushButton::clicked, this, [=] { removeSelected(page.list); });
    connect(page.edit, &QLineEdit::returnPressed, this, [=] {
        if (page.server->currentIndex() < 0) return;
        addServerMapRow(page.list, page.edit->text(), page.server->currentData().toInt());
        page.edit->clear();
    });
    return page;
}

SimpleRouteEditor::ServerMapPage SimpleRouteEditor::makeServerAppPage(QWidget *parent) {
    ServerMapPage page;
    auto *lay = new QVBoxLayout(parent);
    auto *hint = new QLabel(tr("Route specific apps through a chosen server. "
                               "Requires Tun/VPN mode (process matching)."),
                            parent);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#666;");
    lay->addWidget(hint);

    page.list = new QListWidget(parent);
    page.list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    lay->addWidget(page.list, 1);

    auto *row = new QHBoxLayout;
    page.server = new QComboBox(parent);
    page.server->setMinimumWidth(140);
    refillServerCombo(page.server);
    page.edit = new QLineEdit(parent);
    page.edit->setPlaceholderText(tr("opera.exe"));
    auto *addBtn = new QPushButton(tr("Add"), parent);
    auto *browseBtn = new QPushButton(tr("Browse…"), parent);
    auto *runningBtn = new QPushButton(tr("Running…"), parent);
    auto *delBtn = new QPushButton(tr("Remove"), parent);
    row->addWidget(page.server);
    row->addWidget(page.edit, 1);
    row->addWidget(addBtn);
    row->addWidget(browseBtn);
    row->addWidget(runningBtn);
    row->addWidget(delBtn);
    lay->addLayout(row);

    auto addCurrent = [=] {
        if (page.server->currentIndex() < 0) {
            QMessageBox::warning(this, tr("Apps by server"), tr("Select a server first."));
            return;
        }
        addServerMapRow(page.list, page.edit->text(), page.server->currentData().toInt());
        page.edit->clear();
    };
    connect(addBtn, &QPushButton::clicked, this, addCurrent);
    connect(delBtn, &QPushButton::clicked, this, [=] { removeSelected(page.list); });
    connect(page.edit, &QLineEdit::returnPressed, this, addCurrent);
    connect(browseBtn, &QPushButton::clicked, this, [=] {
        auto path = QFileDialog::getOpenFileName(this, tr("Select application"), QString(),
#ifdef Q_OS_WIN
                                                 tr("Programs (*.exe);;All (*.*)")
#else
                                                 tr("All (*.*)")
#endif
        );
        if (path.isEmpty()) return;
        page.edit->setText(QFileInfo(path).fileName());
        addCurrent();
    });
    connect(runningBtn, &QPushButton::clicked, this, [=] {
        if (page.server->currentIndex() < 0) {
            QMessageBox::warning(this, tr("Apps by server"), tr("Select a server first."));
            return;
        }
        ProcessSelectDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            const int profileId = page.server->currentData().toInt();
            for (const auto &proc : dlg.selectedProcessNames()) {
                addServerMapRow(page.list, proc, profileId);
            }
        }
    });
    return page;
}

SimpleRouteEditor::SimpleRouteEditor(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Connection Rules"));
    resize(560, 640);

    auto *root = new QVBoxLayout(this);
    auto *hint = new QLabel(
        tr("Same data as Advanced → Custom Route.\n"
           "Priority (highest first): Sites/Apps by server → Sites → Apps.\n"
           "Restart the tunnel after saving if it asks."),
        this);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#666; margin-bottom:6px;");
    root->addWidget(hint);

    tabs = new QTabWidget(this);

    auto *ds = new QWidget;
    directSites = makeDomainPage(ds, tr(".ru  or  example.com"), &proxySites);
    tabs->addTab(ds, tr("Direct sites"));

    auto *ps = new QWidget;
    proxySites = makeDomainPage(ps, tr(".ru  or  example.com"), &directSites);
    tabs->addTab(ps, tr("Proxy sites"));

    auto *da = new QWidget;
    directApps = makeAppPage(da, tr("chrome.exe"), &proxyApps);
    tabs->addTab(da, tr("Direct apps"));

    auto *pa = new QWidget;
    proxyApps = makeAppPage(pa, tr("Discord.exe"), &directApps);
    tabs->addTab(pa, tr("Proxy apps"));

    auto *ss = new QWidget;
    serverSites = makeServerDomainPage(ss);
    tabs->addTab(ss, tr("Sites by server"));

    auto *sa = new QWidget;
    serverApps = makeServerAppPage(sa);
    tabs->addTab(sa, tr("Apps by server"));

    auto *jsonPage = new QWidget;
    auto *jsonLay = new QVBoxLayout(jsonPage);
    jsonEdit = new QPlainTextEdit(jsonPage);
    jsonEdit->setPlaceholderText(QStringLiteral("{\n  \"rules\": []\n}"));
    jsonEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    jsonLay->addWidget(jsonEdit, 1);
    tabs->addTab(jsonPage, tr("Custom Route JSON"));

    root->addWidget(tabs, 1);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    root->addWidget(box);

    connect(tabs, &QTabWidget::currentChanged, this, [=](int index) {
        if (syncing) return;
        const int jsonIndex = tabs->count() - 1;
        if (lastTabIndex == jsonIndex && index != jsonIndex) {
            if (!syncListsFromJson()) {
                syncing = true;
                tabs->setCurrentIndex(jsonIndex);
                syncing = false;
                return;
            }
        } else if (index == jsonIndex) {
            syncJsonFromLists();
        }
        // Refresh server lists when opening split-routing tabs
        if (index == 4) refillServerCombo(serverSites.server);
        if (index == 5) refillServerCombo(serverApps.server);
        lastTabIndex = index;
    });

    connect(box, &QDialogButtonBox::accepted, this, [=] {
        if (tabs->currentIndex() == tabs->count() - 1) {
            if (!syncListsFromJson()) return;
        } else {
            syncJsonFromLists();
        }
        accept();
    });
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SimpleRouteEditor::clearFriendlyLists() {
    for (auto *list: {directSites.list, proxySites.list, directApps.list, proxyApps.list,
                      serverSites.list, serverApps.list}) {
        if (list) list->clear();
    }
}

void SimpleRouteEditor::parseRulesIntoLists(const QJsonArray &rules) {
    for (const auto &v: rules) {
        if (!v.isObject()) {
            otherRulesJson << QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
            continue;
        }
        auto rule = v.toObject();
        const auto outbound = rule.value("outbound").toString();
        const bool hasDomain = rule.contains("domain_suffix");
        const bool hasProc = rule.contains("process_name");
        const auto keys = rule.keys();
        const QStringList known{"domain_suffix", "process_name", "outbound"};
        bool extra = false;
        for (const auto &k: keys) {
            if (!known.contains(k)) {
                extra = true;
                break;
            }
        }
        if (extra || (hasDomain && hasProc)) {
            otherRulesJson << QString::fromUtf8(QJsonDocument(rule).toJson(QJsonDocument::Compact));
            continue;
        }

        const int splitId = profileIdFromOutboundTag(outbound);

        if (hasDomain) {
            if (splitId >= 0) {
                for (const auto &d: rule.value("domain_suffix").toArray()) {
                    addServerMapRow(serverSites.list, d.toString(), splitId);
                }
                continue;
            }
            if (outbound != "proxy" && outbound != "direct" && !outbound.isEmpty()) {
                otherRulesJson << QString::fromUtf8(QJsonDocument(rule).toJson(QJsonDocument::Compact));
                continue;
            }
            auto *target = (outbound == "proxy") ? proxySites.list : directSites.list;
            for (const auto &d: rule.value("domain_suffix").toArray()) {
                addListRow(target, d.toString());
            }
            continue;
        }
        if (hasProc) {
            if (splitId >= 0) {
                for (const auto &p: rule.value("process_name").toArray()) {
                    addServerMapRow(serverApps.list, p.toString(), splitId);
                }
                continue;
            }
            if (outbound != "proxy" && outbound != "direct" && !outbound.isEmpty()) {
                otherRulesJson << QString::fromUtf8(QJsonDocument(rule).toJson(QJsonDocument::Compact));
                continue;
            }
            auto *target = (outbound == "direct") ? directApps.list : proxyApps.list;
            for (const auto &p: rule.value("process_name").toArray()) {
                addListRow(target, p.toString());
            }
            continue;
        }
        otherRulesJson << QString::fromUtf8(QJsonDocument(rule).toJson(QJsonDocument::Compact));
    }
}

void SimpleRouteEditor::loadFromJson(const QString &json) {
    clearFriendlyLists();
    otherRulesJson.clear();
    refillServerCombo(serverSites.server);
    refillServerCombo(serverApps.server);

    QByteArray raw = json.trimmed().toUtf8();
    if (raw.startsWith("\xEF\xBB\xBF")) raw = raw.mid(3);

    QJsonParseError err{};
    auto doc = QJsonDocument::fromJson(raw, &err);
    QJsonArray rules;
    if (doc.isObject()) {
        rules = doc.object().value("rules").toArray();
    } else if (doc.isArray()) {
        rules = doc.array();
    }
    parseRulesIntoLists(rules);
    syncJsonFromLists();
}

void SimpleRouteEditor::syncJsonFromLists() {
    if (!jsonEdit) return;
    syncing = true;
    jsonEdit->setPlainText(toJson());
    syncing = false;
}

bool SimpleRouteEditor::syncListsFromJson() {
    if (!jsonEdit) return true;
    auto text = jsonEdit->toPlainText().trimmed();
    if (text.isEmpty()) text = QStringLiteral("{\"rules\":[]}");

    QJsonParseError err{};
    auto doc = QJsonDocument::fromJson(text.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || (!doc.isObject() && !doc.isArray())) {
        QMessageBox::warning(this, tr("Custom Route JSON"),
                             tr("Invalid JSON: %1").arg(err.errorString()));
        return false;
    }

    clearFriendlyLists();
    otherRulesJson.clear();
    if (doc.isObject()) {
        parseRulesIntoLists(doc.object().value("rules").toArray());
    } else {
        parseRulesIntoLists(doc.array());
    }
    return true;
}

QString SimpleRouteEditor::toJson() const {
    QJsonArray rules;

    auto pushDomain = [&](QListWidget *list, const char *outbound) {
        auto arr = collectList(list);
        if (!arr.isEmpty()) {
            rules += QJsonObject{{"domain_suffix", arr}, {"outbound", QString::fromUtf8(outbound)}};
        }
    };
    auto pushProc = [&](QListWidget *list, const char *outbound) {
        auto arr = collectList(list);
        if (!arr.isEmpty()) {
            rules += QJsonObject{{"outbound", QString::fromUtf8(outbound)}, {"process_name", arr}};
        }
    };

    // sing-box: first matching rule wins. Priority (highest → lowest):
    // 1) Sites by server  2) Apps by server  3) Direct/Proxy sites  4) Direct/Proxy apps
    auto pushServerMap = [&](QListWidget *list, bool isDomain) {
        if (!list) return;
        QMap<QString, QJsonArray> byOutbound;
        for (int i = 0; i < list->count(); ++i) {
            auto *it = list->item(i);
            const QString matcher = it->data(kRoleMatcher).toString();
            QString outbound = it->data(kRoleOutbound).toString();
            if (outbound.isEmpty()) {
                outbound = profileOutboundTag(it->data(kRoleProfileId).toInt());
            }
            if (matcher.isEmpty() || outbound.isEmpty()) continue;
            byOutbound[outbound] += matcher;
        }
        for (auto it = byOutbound.begin(); it != byOutbound.end(); ++it) {
            if (isDomain) {
                rules += QJsonObject{{"domain_suffix", it.value()}, {"outbound", it.key()}};
            } else {
                rules += QJsonObject{{"outbound", it.key()}, {"process_name", it.value()}};
            }
        }
    };
    pushServerMap(serverSites.list, true);
    pushServerMap(serverApps.list, false);
    // Sites before apps: domain rules beat process (2ip.ru Direct wins over opera.exe Proxy).
    pushDomain(directSites.list, "direct");
    pushDomain(proxySites.list, "proxy");
    pushProc(directApps.list, "direct");
    pushProc(proxyApps.list, "proxy");

    for (const auto &raw: otherRulesJson) {
        auto d = QJsonDocument::fromJson(raw.toUtf8());
        if (d.isObject()) rules += d.object();
    }
    return QString::fromUtf8(QJsonDocument(QJsonObject{{"rules", rules}}).toJson(QJsonDocument::Indented));
}

bool SimpleRouteEditor::addRuleToCustomRoute(const QString &matcher, bool isApp, bool isProxy) {
    QString target = matcher.trimmed();
    if (target.isEmpty()) return false;
    if (!isApp && target.contains(':')) {
        target = target.section(':', 0, 0).trimmed();
    }
    if (target.isEmpty()) return false;

    auto mergeRouteJson = [](const QString &a, const QString &b) {
        QJsonArray rules;
        auto take = [&](const QString &raw) {
            auto obj = QString2QJsonObject(raw.trimmed().isEmpty() ? QStringLiteral("{\"rules\":[]}") : raw);
            for (const auto &v: obj.value("rules").toArray()) rules += v;
        };
        take(a);
        take(b);
        return QJsonObject2QString(QJsonObject{{"rules", rules}}, false);
    };

    SimpleRouteEditor editor;
    editor.loadFromJson(mergeRouteJson(NekoGui::dataStore->routing->custom,
                                       NekoGui::dataStore->custom_route_global));

    QListWidget *targetList = nullptr;
    QListWidget *opposingList = nullptr;

    if (isApp) {
        targetList = isProxy ? editor.proxyApps.list : editor.directApps.list;
        opposingList = isProxy ? editor.directApps.list : editor.proxyApps.list;
    } else {
        targetList = isProxy ? editor.proxySites.list : editor.directSites.list;
        opposingList = isProxy ? editor.directSites.list : editor.proxySites.list;
    }

    if (!targetList) return false;

    // Check if already present in targetList
    for (int i = 0; i < targetList->count(); ++i) {
        if (targetList->item(i)->text().compare(target, Qt::CaseInsensitive) == 0) {
            return false;
        }
    }

    // Remove from opposing list if it was there
    if (opposingList) {
        for (int i = 0; i < opposingList->count(); ++i) {
            if (opposingList->item(i)->text().compare(target, Qt::CaseInsensitive) == 0) {
                delete opposingList->takeItem(i);
                break;
            }
        }
    }

    addListRow(targetList, target);

    const QString out = editor.toJson();
    NekoGui::dataStore->routing->custom = out;
    NekoGui::dataStore->custom_route_global = QStringLiteral("{\"rules\": []}");
    NekoGui::dataStore->routing->Save();
    NekoGui::dataStore->Save();
    return true;
}

