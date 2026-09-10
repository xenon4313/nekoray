#include "DetailsPanel.h"
#include "SimpleRouteEditor.h"
#include "ui/mainwindow.h"
#include "main/NekoGui.hpp"
#include "main/NekoGui_Utils.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QDateTime>
#include <QFileInfo>
#include <QMap>
#include <QSet>
#include <QScrollBar>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QAbstractButton>
#include <algorithm>
#include <functional>

namespace {

struct AggNode {
    QString key;
    QString title;
    qint64 upload = 0;
    qint64 download = 0;
    qint64 lastSeen = 0;
    int active = 0;
    int total = 0;
    QString tag;
    QMap<QString, AggNode> children;
};

QString fmtTraffic(qint64 up, qint64 down) {
    return QStringLiteral("%1↑ %2↓").arg(ReadableSize(up), ReadableSize(down));
}

QString fmtLast(qint64 ts) {
    if (ts <= 0) return QObject::tr("n/a");
    return QDateTime::fromSecsSinceEpoch(ts).toString("HH:mm:ss");
}

void mergeTag(AggNode *node, const QString &tag) {
    if (tag.isEmpty() || node == nullptr) return;
    if (node->tag.isEmpty()) {
        node->tag = tag;
        return;
    }
    if (node->tag == tag) return;
    const auto parts = node->tag.split('/');
    if (!parts.contains(tag)) node->tag += "/" + tag;
}

void addConn(AggNode &root, const QStringList &path, const QJsonObject &obj) {
    AggNode *cur = &root;
    const QString tag = obj["Tag"].toString();
    for (int i = 0; i < path.size(); ++i) {
        const QString &k = path[i];
        if (!cur->children.contains(k)) {
            AggNode n;
            n.key = k;
            n.title = k;
            cur->children.insert(k, n);
        }
        cur = &cur->children[k];
        cur->upload += obj["Upload"].toVariant().toLongLong();
        cur->download += obj["Download"].toVariant().toLongLong();
        qint64 start = obj["Start"].toVariant().toLongLong();
        qint64 end = obj["End"].toVariant().toLongLong();
        qint64 last = end > 0 ? end : start;
        if (last > cur->lastSeen) cur->lastSeen = last;
        cur->total += 1;
        if (obj["Active"].toBool()) cur->active += 1;
        // Outbound on every level (FQDN included), not only the IP leaf
        mergeTag(cur, tag);
    }
}

QList<AggNode *> sortedChildren(AggNode &node, int sortMode) {
    QList<AggNode *> list;
    for (auto it = node.children.begin(); it != node.children.end(); ++it) {
        list << &it.value();
    }
    std::sort(list.begin(), list.end(), [sortMode](AggNode *a, AggNode *b) {
        if (sortMode == 1) { // last
            if (a->lastSeen != b->lastSeen) return a->lastSeen > b->lastSeen;
        } else if (sortMode == 2) { // traffic
            auto ta = a->upload + a->download;
            auto tb = b->upload + b->download;
            if (ta != tb) return ta > tb;
        }
        return QString::compare(a->title, b->title, Qt::CaseInsensitive) < 0;
    });
    return list;
}

void fillTree(QTreeWidget *tree, QTreeWidgetItem *parent, AggNode &node, int sortMode, int depth,
              const QString &parentPath, const QSet<QString> &expandedKeys, bool hadSavedState, int mode) {
    auto kids = sortedChildren(node, sortMode);
    for (AggNode *c: kids) {
        auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
        QString prefix;
        int kind = 0; // 1: Process, 2: Host, 3: IP
        if (mode == 1) { // ByHostIp: 0 = Host, 1 = IP
            kind = (depth == 0) ? 2 : 3;
            prefix = (depth == 0) ? "" : QObject::tr("IP: ");
        } else if (mode == 2) { // ByProcessIp: 0 = Process, 1 = IP
            kind = (depth == 0) ? 1 : 3;
            prefix = (depth == 0) ? "" : QObject::tr("IP: ");
        } else { // ByProcessHostIp: 0 = Process, 1 = Host, 2 = IP
            if (depth == 0) { kind = 1; prefix = ""; }
            else if (depth == 1) { kind = 2; prefix = QObject::tr("FQDN: "); }
            else { kind = 3; prefix = QObject::tr("IP: "); }
        }

        QString status;
        if (c->active > 0) status = QObject::tr("%1 active").arg(c->active);
        else status = QObject::tr("idle");

        const QString pathKey = parentPath.isEmpty() ? c->key : (parentPath + QChar(1) + c->key);
        item->setData(0, Qt::UserRole, pathKey);
        item->setData(0, Qt::UserRole + 1, c->title);
        item->setData(0, Qt::UserRole + 2, kind);
        item->setText(0, prefix + c->title);
        item->setText(1, c->tag);
        item->setText(2, status);
        item->setText(3, fmtLast(c->lastSeen));
        item->setText(4, fmtTraffic(c->upload, c->download));
        item->setToolTip(0, c->title);

        if (expandedKeys.contains(pathKey)) {
            item->setExpanded(true);
        } else if (!hadSavedState && depth < 1) {
            item->setExpanded(true);
        } else {
            item->setExpanded(false);
        }

        fillTree(tree, item, *c, sortMode, depth + 1, pathKey, expandedKeys, hadSavedState, mode);
    }
}

bool nodeMatches(const AggNode &node, const QString &q) {
    if (q.isEmpty()) return true;
    if (node.title.contains(q, Qt::CaseInsensitive)) return true;
    for (auto it = node.children.begin(); it != node.children.end(); ++it) {
        if (nodeMatches(it.value(), q)) return true;
    }
    return false;
}

void filterNode(AggNode &node, const QString &q) {
    if (q.isEmpty()) return;
    QStringList remove;
    for (auto it = node.children.begin(); it != node.children.end(); ++it) {
        if (!nodeMatches(it.value(), q)) remove << it.key();
        else filterNode(it.value(), q);
    }
    for (const auto &k: remove) node.children.remove(k);
}

QSet<QString> collectExpandedKeys(QTreeWidget *tree) {
    QSet<QString> keys;
    std::function<void(QTreeWidgetItem *)> walk = [&](QTreeWidgetItem *it) {
        if (it == nullptr) return;
        if (it->isExpanded()) {
            const auto k = it->data(0, Qt::UserRole).toString();
            if (!k.isEmpty()) keys.insert(k);
        }
        for (int i = 0; i < it->childCount(); ++i) walk(it->child(i));
    };
    for (int i = 0; i < tree->topLevelItemCount(); ++i) walk(tree->topLevelItem(i));
    return keys;
}

} // namespace

DetailsPanel::DetailsPanel(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto *bar = new QHBoxLayout;
    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText(tr("Search process / FQDN / IP…"));
    searchEdit->setClearButtonEnabled(true);

    groupCombo = new QComboBox(this);
    groupCombo->addItem(tr("Process → FQDN → IP"), ByProcessHostIp);
    groupCombo->addItem(tr("FQDN → IP"), ByHostIp);
    groupCombo->addItem(tr("Process → IP"), ByProcessIp);

    sortCombo = new QComboBox(this);
    sortCombo->addItem(tr("Sort: Name"), SortByName);
    sortCombo->addItem(tr("Sort: Last seen"), SortByLast);
    sortCombo->addItem(tr("Sort: Traffic"), SortByTraffic);

    activeOnly = new QCheckBox(tr("Active only"), this);

    bar->addWidget(searchEdit, 1);
    bar->addWidget(groupCombo);
    bar->addWidget(sortCombo);
    bar->addWidget(activeOnly);
    layout->addLayout(bar);

    tree = new QTreeWidget(this);
    tree->setHeaderLabels({tr("Name"), tr("Outbound"), tr("Status"), tr("Last"), tr("Traffic")});
    tree->setUniformRowHeights(true);
    tree->setAlternatingRowColors(true);
    tree->setRootIsDecorated(true);
    tree->setAnimated(true);
    tree->header()->setStretchLastSection(false);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree, &QTreeWidget::customContextMenuRequested, this, &DetailsPanel::onContextMenu);
    layout->addWidget(tree, 1);

    summary = new QLabel(this);
    layout->addWidget(summary);

    connect(searchEdit, &QLineEdit::textChanged, this, &DetailsPanel::rebuildTree);
    connect(groupCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DetailsPanel::rebuildTree);
    connect(sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DetailsPanel::rebuildTree);
    connect(activeOnly, &QCheckBox::toggled, this, &DetailsPanel::rebuildTree);
}

void DetailsPanel::updateConnections(const QJsonArray &arr) {
    lastConnections = arr;
    rebuildTree();
}

void DetailsPanel::rebuildTree() {
    const QSet<QString> expandedKeys = collectExpandedKeys(tree);
    const bool hadSavedState = !expandedKeys.isEmpty();
    const int scrollPos = tree->verticalScrollBar() ? tree->verticalScrollBar()->value() : 0;
    QString selectedKey;
    if (auto *sel = tree->currentItem()) selectedKey = sel->data(0, Qt::UserRole).toString();

    tree->clear();
    AggNode root;
    root.title = "root";

    auto mode = (GroupMode) groupCombo->currentData().toInt();
    auto sortMode = sortCombo->currentData().toInt();
    auto onlyActive = activeOnly->isChecked();
    auto query = searchEdit->text().trimmed();

    int connCount = 0;
    int activeCount = 0;
    qint64 totalUp = 0, totalDown = 0;

    for (const auto &v: lastConnections) {
        auto obj = v.toObject();
        bool active = obj["Active"].toBool();
        if (onlyActive && !active) continue;

        QString process = obj["Process"].toString();
        if (process.isEmpty()) process = "(unknown)";
        QString host = obj["Host"].toString();
        if (host.isEmpty()) host = obj["Dest"].toString();
        QString ip = obj["DestIP"].toString();
        if (ip.isEmpty()) ip = host;
        if (obj["DestPort"].toInt() > 0) {
            ip = QStringLiteral("%1:%2").arg(ip).arg(obj["DestPort"].toInt());
        }

        QStringList path;
        switch (mode) {
            case ByHostIp:
                path = {host, ip};
                break;
            case ByProcessIp:
                path = {process, ip};
                break;
            case ByProcessHostIp:
            default:
                path = {process, host, ip};
                break;
        }
        addConn(root, path, obj);
        connCount++;
        if (active) activeCount++;
        totalUp += obj["Upload"].toVariant().toLongLong();
        totalDown += obj["Download"].toVariant().toLongLong();
    }

    filterNode(root, query);
    fillTree(tree, nullptr, root, sortMode, 0, {}, expandedKeys, hadSavedState, static_cast<int>(mode));

    if (!selectedKey.isEmpty()) {
        std::function<QTreeWidgetItem *(QTreeWidgetItem *)> find = [&](QTreeWidgetItem *it) -> QTreeWidgetItem * {
            if (it == nullptr) return nullptr;
            if (it->data(0, Qt::UserRole).toString() == selectedKey) return it;
            for (int i = 0; i < it->childCount(); ++i) {
                if (auto *f = find(it->child(i))) return f;
            }
            return nullptr;
        };
        QTreeWidgetItem *found = nullptr;
        for (int i = 0; i < tree->topLevelItemCount() && !found; ++i) found = find(tree->topLevelItem(i));
        if (found) tree->setCurrentItem(found);
    }
    if (tree->verticalScrollBar()) tree->verticalScrollBar()->setValue(scrollPos);

    summary->setText(tr("Connections: %1 (%2 active)  •  %3")
                         .arg(connCount)
                         .arg(activeCount)
                         .arg(fmtTraffic(totalUp, totalDown)));
}

void DetailsPanel::onContextMenu(const QPoint &pos) {
    auto *item = tree->itemAt(pos);
    if (!item) return;

    QString rawName = item->data(0, Qt::UserRole + 1).toString().trimmed();
    int kind = item->data(0, Qt::UserRole + 2).toInt();
    if (rawName.isEmpty()) return;

    QMenu menu(this);

    QString parentProcess;
    QString parentHost;
    if (auto *p = item->parent()) {
        int pKind = p->data(0, Qt::UserRole + 2).toInt();
        if (pKind == 1) parentProcess = p->data(0, Qt::UserRole + 1).toString().trimmed();
        else if (pKind == 2) parentHost = p->data(0, Qt::UserRole + 1).toString().trimmed();

        if (auto *gp = p->parent()) {
            int gpKind = gp->data(0, Qt::UserRole + 2).toInt();
            if (gpKind == 1) parentProcess = gp->data(0, Qt::UserRole + 1).toString().trimmed();
        }
    }

    if (kind == 1) { // Process
        if (rawName != "(unknown)") {
            menu.addAction(tr("Add \"%1\" to Direct Apps").arg(rawName), this, [=] {
                addRuleAndPrompt(rawName, true, false);
            });
            menu.addAction(tr("Add \"%1\" to Proxy Apps").arg(rawName), this, [=] {
                addRuleAndPrompt(rawName, true, true);
            });
        }
    } else if (kind == 2) { // Host / FQDN
        menu.addAction(tr("Add \"%1\" to Direct Sites").arg(rawName), this, [=] {
            addRuleAndPrompt(rawName, false, false);
        });
        menu.addAction(tr("Add \"%1\" to Proxy Sites").arg(rawName), this, [=] {
            addRuleAndPrompt(rawName, false, true);
        });
        if (!parentProcess.isEmpty() && parentProcess != "(unknown)") {
            menu.addSeparator();
            menu.addAction(tr("Add process \"%1\" to Direct Apps").arg(parentProcess), this, [=] {
                addRuleAndPrompt(parentProcess, true, false);
            });
            menu.addAction(tr("Add process \"%1\" to Proxy Apps").arg(parentProcess), this, [=] {
                addRuleAndPrompt(parentProcess, true, true);
            });
        }
    } else if (kind == 3) { // IP
        if (!parentHost.isEmpty()) {
            menu.addAction(tr("Add \"%1\" to Direct Sites").arg(parentHost), this, [=] {
                addRuleAndPrompt(parentHost, false, false);
            });
            menu.addAction(tr("Add \"%1\" to Proxy Sites").arg(parentHost), this, [=] {
                addRuleAndPrompt(parentHost, false, true);
            });
        }
        if (!parentProcess.isEmpty() && parentProcess != "(unknown)") {
            if (!parentHost.isEmpty()) menu.addSeparator();
            menu.addAction(tr("Add process \"%1\" to Direct Apps").arg(parentProcess), this, [=] {
                addRuleAndPrompt(parentProcess, true, false);
            });
            menu.addAction(tr("Add process \"%1\" to Proxy Apps").arg(parentProcess), this, [=] {
                addRuleAndPrompt(parentProcess, true, true);
            });
        }
    }

    if (!menu.isEmpty()) {
        menu.exec(tree->viewport()->mapToGlobal(pos));
    }
}

void DetailsPanel::addRuleAndPrompt(const QString &matcher, bool isApp, bool isProxy) {
    if (matcher.isEmpty()) return;

    QString ruleType = isApp ? (isProxy ? tr("Proxy apps") : tr("Direct apps"))
                             : (isProxy ? tr("Proxy sites") : tr("Direct sites"));

    bool added = SimpleRouteEditor::addRuleToCustomRoute(matcher, isApp, isProxy);
    if (!added) {
        QMessageBox::information(this, tr("Connection Rules"),
                                 tr("\"%1\" is already in %2.").arg(matcher, ruleType));
        return;
    }

    QMessageBox box(this);
    box.setWindowTitle(tr("Connection Rules"));
    box.setIcon(QMessageBox::Information);
    box.setText(tr("Added \"%1\" to %2.\n\nRestart the tunnel to apply changes.")
                    .arg(matcher, ruleType));

    auto *restartBtn = box.addButton(tr("Restart Tunnel"), QMessageBox::AcceptRole);
    auto *laterBtn = box.addButton(tr("Later"), QMessageBox::RejectRole);
    box.setDefaultButton(restartBtn);
    box.exec();

    if (box.clickedButton() == restartBtn) {
        if (NekoGui::dataStore->started_id >= 0 && GetMainWindow()) {
            GetMainWindow()->neko_start(NekoGui::dataStore->started_id);
        }
        MW_dialog_message("", "UpdateDataStore");
    } else {
        MW_dialog_message("", "UpdateDataStore");
    }
}

