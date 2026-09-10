#include "SimpleModeWidget.h"
#include "StatsPanel.h"



#include "db/Database.hpp"
#include "main/NekoGui.hpp"



#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QResizeEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPalette>
#include <QSizePolicy>
#include <QFontMetrics>
#include <QColor>



static void makeTransparent(QWidget *w) {
    if (!w) return;
    w->setAttribute(Qt::WA_TranslucentBackground, true);
    w->setAutoFillBackground(false);
    QPalette pal = w->palette();
    pal.setColor(QPalette::Window, Qt::transparent);
    pal.setColor(QPalette::Base, Qt::transparent);
    w->setPalette(pal);
}



SimpleModeWidget::SimpleModeWidget(QWidget *parent) : QWidget(parent) {
    setObjectName("SimpleModeWidget");
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setFocusPolicy(Qt::StrongFocus);



    advancedBtn = new QPushButton(tr("Advanced"), this);
    updateBtn = new QPushButton(tr("Update"), this);
    updateBtn->hide();
    bgBtn = new QPushButton(tr("Theme"), this);
    rulesBtn = new QPushButton(tr("Rules"), this);
    testBtn = new QPushButton(tr("Test"), this);
    for (auto *b: {advancedBtn, updateBtn, bgBtn, rulesBtn, testBtn}) {
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(28);
        applyChromeButtonStyle(b);
    }



    speedProxy = new QLabel(this);
    speedDirect = new QLabel(this);
    for (auto *l: {speedProxy, speedDirect}) {
        l->setAlignment(Qt::AlignCenter);
        l->setStyleSheet("font-size: 11px; font-weight: bold; color: #000000; background: transparent;");
    }
    updateSpeedLabels(0, 0, 0, 0);



    hero = new QFrame(this);
    hero->setObjectName("heroBlock");
    hero->setCursor(Qt::PointingHandCursor);
    hero->installEventFilter(this);
    hero->setStyleSheet(
        "#heroBlock {"
        "  background: rgba(10,12,18,150);"
        "  border-radius: 18px;"
        "  border: 1px solid rgba(255,255,255,35);"
        "}");



    auto *heroGrid = new QGridLayout(hero);
    heroGrid->setContentsMargins(10, 8, 10, 8);
    heroGrid->setSpacing(0);



    heroChart = new TrafficSparkline(hero);
    heroChart->setTransparentBackground(true);
    heroChart->setShowWindowLabel(false);
    heroChart->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    heroChart->setMinimumHeight(40);



    auto *overlay = new QWidget(hero);
    overlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    makeTransparent(overlay);
    auto *overlayLay = new QVBoxLayout(overlay);
    overlayLay->setContentsMargins(8, 4, 8, 4);
    overlayLay->setSpacing(2);



    heroTitle = new QLabel(tr("Подключись"), overlay);
    heroTitle->setAlignment(Qt::AlignCenter);
    heroTitle->setWordWrap(true);
    heroTitle->setStyleSheet(
        "color: #ffffff;"
        "font-weight: 700;"
        "letter-spacing: 1px;"
        "background: transparent;");



    overlayLay->addStretch(1);
    overlayLay->addWidget(heroTitle);
    overlayLay->addStretch(1);



    heroGrid->addWidget(heroChart, 0, 0);
    heroGrid->addWidget(overlay, 0, 0);



    // Status lives under the hero block (not inside it)
    heroSub = new QLabel(tr("Not connected"), this);
    heroSub->setAlignment(Qt::AlignCenter);
    heroSub->setStyleSheet("color: #e8a4a4; font-size: 13px; font-weight: 600; background: transparent;");



    serverScroll = new QScrollArea(this);
    serverScroll->setObjectName("serverScroll");
    serverScroll->setWidgetResizable(true);
    serverScroll->setFrameShape(QFrame::NoFrame);
    serverScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    serverScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    makeTransparent(serverScroll);
    makeTransparent(serverScroll->viewport());
    serverScroll->setStyleSheet(
        "QScrollArea#serverScroll { background: transparent; border: none; }"
        "QScrollArea#serverScroll > QWidget { background: transparent; }"
        "QScrollBar:vertical {"
        "  width: 5px; background: transparent; margin: 2px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: rgba(255,255,255,70); border-radius: 2px; min-height: 24px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }");



    serverListHost = new QWidget;
    serverListHost->setObjectName("serverListHost");
    makeTransparent(serverListHost);
    serverListHost->setStyleSheet("#serverListHost { background: transparent; }");
    serverListHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    serverListLayout = new QVBoxLayout(serverListHost);
    serverListLayout->setContentsMargins(12, 4, 12, 8);
    serverListLayout->setSpacing(5);
    serverListLayout->setAlignment(Qt::AlignTop);
    serverScroll->setWidget(serverListHost);



    groupScroll = new QScrollArea(this);
    groupScroll->setObjectName("simpleGroupScroll");
    groupScroll->setFrameShape(QFrame::NoFrame);
    groupScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    groupScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    groupScroll->setWidgetResizable(true);
    makeTransparent(groupScroll);
    makeTransparent(groupScroll->viewport());
    groupScroll->setStyleSheet(
        "QScrollArea#simpleGroupScroll { background: transparent; border: none; }"
        "QScrollBar:horizontal { height: 4px; background: transparent; }"
        "QScrollBar::handle:horizontal { background: rgba(255,255,255,70); border-radius: 2px; }");

    groupBar = new QWidget;
    makeTransparent(groupBar);
    groupBarLayout = new QHBoxLayout(groupBar);
    groupBarLayout->setContentsMargins(12, 0, 12, 0);
    groupBarLayout->setSpacing(6);
    groupScroll->setWidget(groupBar);



    connect(advancedBtn, &QPushButton::clicked, this, &SimpleModeWidget::requestAdvancedMode);
    connect(updateBtn, &QPushButton::clicked, this, &SimpleModeWidget::requestCheckUpdate);
    connect(bgBtn, &QPushButton::clicked, this, &SimpleModeWidget::requestToggleBackground);
    connect(rulesBtn, &QPushButton::clicked, this, &SimpleModeWidget::requestEditRules);
    connect(testBtn, &QPushButton::clicked, this, &SimpleModeWidget::requestUrlTest);



    proxyUpHist.reserve(kHistWindow);
    proxyDownHist.reserve(kHistWindow);
    directUpHist.reserve(kHistWindow);
    directDownHist.reserve(kHistWindow);



    reloadBackground();
    refreshGroupTabs();
    refreshServers();
    refreshPowerState();
}



void SimpleModeWidget::applyChromeButtonStyle(QPushButton *b) {
    b->setStyleSheet(
        "QPushButton {"
        "  background: rgba(12,14,20,170);"
        "  color: #f0f0f3;"
        "  border: 1px solid rgba(255,255,255,40);"
        "  border-radius: 9px;"
        "  padding: 3px 10px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background: rgba(22,26,36,200); }"
        "QPushButton:disabled { color: #888890; }");
}



void SimpleModeWidget::applyServerCardStyle(QPushButton *card, bool checked) {
    Q_UNUSED(checked)
    card->setStyleSheet(
        "QPushButton {"
        "  background: rgba(10,12,18,155);"
        "  color: #f0f0f3;"
        "  border: 1px solid rgba(255,255,255,38);"
        "  border-radius: 10px;"
        "  padding: 4px 12px;"
        "  text-align: left;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:checked {"
        "  background: rgba(18,28,48,200);"
        "  border: 1px solid rgba(140,170,220,140);"
        "}"
        "QPushButton:hover {"
        "  background: rgba(18,22,32,190);"
        "}");
}



void SimpleModeWidget::applyLatencyLabel(QLabel *label, int latency) {
    if (!label) return;
    QString text;
    QColor color(200, 200, 210);
    if (latency < 0) {
        text = tr("Unavailable");
        color = QColor(220, 90, 90);
    } else if (latency > 0) {
        text = QStringLiteral("%1 ms").arg(latency);
        const int greenMs = NekoGui::dataStore->test_latency_url.startsWith("https://") ? 200 : 100;
        color = latency < greenMs ? QColor(110, 190, 140) : QColor(210, 180, 90);
    }
    label->setText(text);
    label->setStyleSheet(QStringLiteral(
                             "color: %1; font-size: 11px; font-weight: 600; background: transparent;")
                             .arg(color.name()));
}



QRect SimpleModeWidget::mapDesignRect(int x, int y, int w, int h) const {
    if (width() <= 0 || height() <= 0) return {};
    double scale = qMax((double) width() / kDesignW, (double) height() / kDesignH);
    double drawW = kDesignW * scale;
    double drawH = kDesignH * scale;
    double ox = (width() - drawW) / 2.0;
    double oy = (height() - drawH) / 2.0;
    return QRect(qRound(ox + x * scale), qRound(oy + y * scale), qRound(w * scale), qRound(h * scale));
}



void SimpleModeWidget::reloadBackground() {
    int ver = NekoGui::dataStore->ui_simple_bg;
    bg = QPixmap();
    if (ver == 0 && !NekoGui::dataStore->ui_simple_bg_custom.isEmpty()) {
        bg = QPixmap(NekoGui::dataStore->ui_simple_bg_custom);
    }
    if (bg.isNull() && ver >= 1 && ver <= 4) {
        QString rPath = QStringLiteral(":/neko/ver%1.jpg").arg(ver);
        bg = QPixmap(rPath);
        if (bg.isNull()) {
            bg = QPixmap(QStringLiteral("ver%1.jpg").arg(ver));
        }
    }
    if (bg.isNull()) {
        bg = QPixmap(":/neko/ver1.jpg");
        if (bg.isNull()) bg = QPixmap("ver1.jpg");
    }
    update();
}



int SimpleModeWidget::selectedProfileId() const {
    return selectedId;
}



void SimpleModeWidget::setUrlTestBusy(bool busy) {
    urlTestBusy = busy;
    if (testBtn) {
        testBtn->setEnabled(!busy);
        testBtn->setText(busy ? tr("Testing…") : tr("Test"));
        testBtn->adjustSize();
        rebuildChromeLayout();
    }
}



void SimpleModeWidget::setDisconnecting(bool on) {
    disconnecting = on;
    if (hero) {
        hero->setEnabled(!on);
        hero->setCursor(on ? Qt::BusyCursor : Qt::PointingHandCursor);
    }
    for (auto *c: serverCards) {
        if (c) c->setEnabled(!on);
    }
    updateHeroText();
}



bool SimpleModeWidget::isConnectionBusy() const {
    return disconnecting;
}



void SimpleModeWidget::updateSpeedLabels(qint64 proxyUpRate, qint64 proxyDownRate,
                                         qint64 directUpRate, qint64 directDownRate) {
    speedProxy->setText(QStringLiteral("<span style='color:#000000; font-weight:bold;'>Proxy ↑ %1  ↓ %2</span>")
                            .arg(ReadableSize(proxyUpRate) + "/s",
                                 ReadableSize(proxyDownRate) + "/s"));
    speedDirect->setText(QStringLiteral("<span style='color:#000000; font-weight:bold;'>Direct ↑ %1  ↓ %2</span>")
                             .arg(ReadableSize(directUpRate) + "/s",
                                  ReadableSize(directDownRate) + "/s"));
}



void SimpleModeWidget::pushTrafficSample(qint64 proxyUpRate, qint64 proxyDownRate,
                                         qint64 directUpRate, qint64 directDownRate) {
    auto push = [](QVector<double> &v, double x) {
        v.append(x);
        while (v.size() > kHistWindow) v.removeFirst();
    };
    push(proxyUpHist, (double) proxyUpRate);
    push(proxyDownHist, (double) proxyDownRate);
    push(directUpHist, (double) directUpRate);
    push(directDownHist, (double) directDownRate);



    updateSpeedLabels(proxyUpRate, proxyDownRate, directUpRate, directDownRate);



    if (heroChart) {
        heroChart->setMultiSeries(
            {proxyUpHist, proxyDownHist, directUpHist, directDownHist},
            {TrafficSparkline::colorProxyUp(), TrafficSparkline::colorProxyDown(),
             TrafficSparkline::colorDirectUp(), TrafficSparkline::colorDirectDown()});
    }
}



void SimpleModeWidget::updateServerLatency(int profileId) {
    for (int i = 0; i < serverCards.size(); ++i) {
        auto *card = serverCards[i];
        const int id = card->property("profileId").toInt();
        if (profileId >= 0 && id != profileId) continue;
        auto pf = NekoGui::profileManager->GetProfile(id);
        if (!pf || i >= serverLatencyLabels.size()) continue;
        applyLatencyLabel(serverLatencyLabels[i], pf->latency);
    }
}



void SimpleModeWidget::refreshGroupTabs() {
    if (!groupBarLayout) return;
    while (groupBarLayout->count() > 0) {
        auto *item = groupBarLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    groupTabButtons.clear();

    const int currentGid = NekoGui::dataStore->current_group;
    for (const auto gid: NekoGui::profileManager->groupsTabOrder) {
        auto group = NekoGui::profileManager->GetGroup(gid);
        if (!group) continue;
        auto *b = new QPushButton(group->name, groupBar);
        b->setCheckable(true);
        b->setChecked(gid == currentGid);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(26);
        applyChromeButtonStyle(b);
        if (gid == currentGid) {
            b->setStyleSheet(
                "QPushButton {"
                "  background: rgba(28,36,56,210);"
                "  color: #f0f0f3;"
                "  border: 1px solid rgba(140,170,220,120);"
                "  border-radius: 9px;"
                "  padding: 2px 10px;"
                "  font-weight: 600;"
                "}"
                "QPushButton:hover { background: rgba(32,40,60,220); }");
        }
        connect(b, &QPushButton::clicked, this, [=] {
            if (gid == NekoGui::dataStore->current_group) return;
            emit requestSelectGroup(gid);
        });
        groupBarLayout->addWidget(b);
        groupTabButtons << b;
    }
    groupBarLayout->addStretch(1);
    groupBar->adjustSize();
}



void SimpleModeWidget::refreshServers() {
    // Fully clear layout — no leftover stretch / ghost widgets
    while (serverListLayout->count() > 0) {
        auto *item = serverListLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    serverCards.clear();
    serverLatencyLabels.clear();



    if (selectedId < 0) {
        selectedId = NekoGui::dataStore->simple_last_profile_id;
        if (selectedId < 0) selectedId = NekoGui::dataStore->remember_id;
        if (selectedId < 0) selectedId = NekoGui::dataStore->started_id;
    }



    auto group = NekoGui::profileManager->CurrentGroup();
    if (!group) {
        serverListHost->adjustSize();
        return;
    }



    for (const auto &pf: group->ProfilesWithOrder()) {
        auto *card = new QPushButton(serverListHost);
        card->setProperty("profileId", pf->id);
        card->setCheckable(true);
        card->setChecked(pf->id == selectedId);
        card->setFixedHeight(34);
        card->setCursor(Qt::PointingHandCursor);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        applyServerCardStyle(card, card->isChecked());



        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(4, 0, 4, 0);
        row->setSpacing(8);



        auto *name = new QLabel(pf->bean->DisplayName(), card);
        name->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        name->setStyleSheet("color: #f0f0f3; font-size: 13px; font-weight: 600; background: transparent;");
        name->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);



        auto *lat = new QLabel(card);
        lat->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        lat->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lat->setMinimumWidth(72);
        applyLatencyLabel(lat, pf->latency);



        row->addWidget(name, 1);
        row->addWidget(lat, 0);



        connect(card, &QPushButton::clicked, this, [=] {
            if (disconnecting) return;
            selectedId = pf->id;
            NekoGui::dataStore->simple_last_profile_id = pf->id;
            NekoGui::dataStore->Save();
            for (auto *c: serverCards) {
                bool on = c->property("profileId").toInt() == selectedId;
                c->setChecked(on);
            }
            // Clicking the already-connected profile = disconnect
            const bool connectedToThis = NekoGui::dataStore->started_id == pf->id;
            emit requestPowerToggle(!connectedToThis, selectedId);
        });



        serverListLayout->addWidget(card);
        serverCards << card;
        serverLatencyLabels << lat;
    }



    serverListHost->adjustSize();
}



void SimpleModeWidget::fitHeroTitleFont() {
    if (!heroTitle || !hero) return;
    const QString text = heroTitle->text();
    if (text.isEmpty()) return;



    const int maxW = qMax(40, hero->width() - 36);
    const int maxH = qMax(24, hero->height() / 2);
    QFont f = heroTitle->font();
    f.setBold(true);



    int chosen = 10;
    for (int pt = 24; pt >= 10; --pt) {
        f.setPointSize(pt);
        QFontMetrics fm(f);
        QRect br = fm.boundingRect(QRect(0, 0, maxW, maxH * 2), Qt::AlignCenter | Qt::TextWordWrap, text);
        if (br.width() <= maxW && br.height() <= maxH) {
            chosen = pt;
            break;
        }
    }
    f.setPointSize(chosen);
    heroTitle->setFont(f);
}



void SimpleModeWidget::updateHeroText() {
    if (disconnecting) {
        heroTitle->setText(tr("Отключаюсь"));
        heroSub->setText(tr("Disconnecting…"));
        heroSub->setStyleSheet("color: #e8c48a; font-size: 13px; font-weight: 600; background: transparent;");
        fitHeroTitleFont();
        return;
    }
    bool on = NekoGui::dataStore->started_id >= 0;
    if (on) {
        auto ent = NekoGui::profileManager->GetProfile(NekoGui::dataStore->started_id);
        QString name = ent ? ent->bean->DisplayName() : QString::number(NekoGui::dataStore->started_id);
        heroTitle->setText(name);
        heroSub->setText(tr("Connected"));
        heroSub->setStyleSheet("color: #8fceb0; font-size: 13px; font-weight: 600; background: transparent;");
    } else {
        heroTitle->setText(tr("Подключись"));
        heroSub->setText(tr("Not connected"));
        heroSub->setStyleSheet("color: #e8a4a4; font-size: 13px; font-weight: 600; background: transparent;");
    }
    fitHeroTitleFont();
}



void SimpleModeWidget::refreshPowerState() {
    // Stop finished — clear disconnecting lock and show "Подключись"
    if (disconnecting && NekoGui::dataStore->started_id < 0) {
        disconnecting = false;
        if (hero) {
            hero->setEnabled(true);
            hero->setCursor(Qt::PointingHandCursor);
        }
        for (auto *c: serverCards) {
            if (c) c->setEnabled(true);
        }
    }
    bool on = NekoGui::dataStore->started_id >= 0;
    if (on && !disconnecting) {
        selectedId = NekoGui::dataStore->started_id;
        for (auto *c: serverCards) {
            c->setChecked(c->property("profileId").toInt() == selectedId);
        }
    }
    updateHeroText();
}



void SimpleModeWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    if (!bg.isNull()) {
        double scale = qMax((double) width() / kDesignW, (double) height() / kDesignH);
        int w = (int) (kDesignW * scale);
        int h = (int) (kDesignH * scale);
        QRect dest((width() - w) / 2, (height() - h) / 2, w, h);

        // Aspect-fill (cover) preserving source aspect ratio without stretching
        double targetAspect = (double) w / (double) h;
        double bgAspect = (double) bg.width() / (double) bg.height();
        QRect src;
        if (bgAspect > targetAspect) {
            int srcW = qRound(bg.height() * targetAspect);
            int srcX = (bg.width() - srcW) / 2;
            src = QRect(srcX, 0, srcW, bg.height());
        } else {
            int srcH = qRound(bg.width() / targetAspect);
            int srcY = (bg.height() - srcH) / 2;
            src = QRect(0, srcY, bg.width(), srcH);
        }
        p.drawPixmap(dest, bg, src);
    } else {
        p.fillRect(rect(), QColor(28, 30, 38));
    }
}



void SimpleModeWidget::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    rebuildChromeLayout();
}



bool SimpleModeWidget::eventFilter(QObject *obj, QEvent *event) {
    if (obj == hero && event->type() == QEvent::MouseButtonRelease) {
        if (disconnecting) return true;
        bool on = NekoGui::dataStore->started_id >= 0;
        int id = selectedId;
        if (id < 0 && !serverCards.isEmpty()) {
            id = serverCards.first()->property("profileId").toInt();
        }
        emit requestPowerToggle(!on, id);
        return true;
    }
    return QWidget::eventFilter(obj, event);
}



void SimpleModeWidget::keyPressEvent(QKeyEvent *event) {
    if (event->matches(QKeySequence::Paste)) {
        emit requestPasteFromClipboard();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}



void SimpleModeWidget::rebuildChromeLayout() {
    const int m = 12;
    advancedBtn->adjustSize();
    updateBtn->adjustSize();
    rulesBtn->adjustSize();
    bgBtn->adjustSize();
    testBtn->adjustSize();



    advancedBtn->move(m, m);
    // updateBtn->move(advancedBtn->x() + advancedBtn->width() + 8, m);
    bgBtn->move(width() - m - bgBtn->width(), m);
    rulesBtn->move(bgBtn->x() - 8 - rulesBtn->width(), m);
    advancedBtn->raise();
    // updateBtn->raise();
    rulesBtn->raise();
    bgBtn->raise();



    QRect heroR = mapDesignRect(kHeroX, kHeroY, kHeroW, kHeroH);
    heroR = heroR.intersected(rect().adjusted(8, 56, -8, -8));
    if (heroR.height() < 72) heroR.setHeight(72);
    hero->setGeometry(heroR);
    hero->raise();
    fitHeroTitleFont();



    // Speeds sit just above the hero / server-name block
    speedProxy->adjustSize();
    speedDirect->adjustSize();
    const int speedGap = 2;
    const int speedH = speedProxy->sizeHint().height() + speedDirect->sizeHint().height() + speedGap;
    const int speedY = qMax(m + 32, heroR.top() - speedH - 8);
    const int speedW = qMin(width() - 24, qMax(heroR.width(), 220));
    const int speedX = (width() - speedW) / 2;
    speedProxy->setGeometry(speedX, speedY, speedW, speedProxy->sizeHint().height());
    speedDirect->setGeometry(speedX, speedY + speedProxy->height() + speedGap, speedW, speedDirect->sizeHint().height());
    speedProxy->raise();
    speedDirect->raise();



    heroSub->adjustSize();
    const int subW = qMin(width() - 24, qMax(heroR.width(), heroSub->sizeHint().width() + 20));
    const int subX = (width() - subW) / 2;
    const int subY = heroR.bottom() + 8;
    heroSub->setGeometry(subX, subY, subW, heroSub->sizeHint().height());
    heroSub->raise();



    // Full-width list, pinned to bottom (taller than before)
    const int listMaxH = qMax(88, (int) (height() * kListHeightFraction));
    const int gap = 10;
    const int testH = testBtn->sizeHint().height();
    const int groupH = 28;
    int listBottom = height() - 10;
    int listTop = listBottom - listMaxH;
    const int minTop = heroSub->geometry().bottom() + gap + groupH + 6 + testH + 6;
    if (listTop < minTop) listTop = minTop;
    int listH = listBottom - listTop;
    if (listH > 40) {
        const int groupY = listTop - groupH - 4;
        groupScroll->setGeometry(0, groupY, width(), groupH);
        groupScroll->show();
        groupScroll->raise();

        serverScroll->setGeometry(0, listTop, width(), listH);
        serverScroll->show();
        serverScroll->raise();
        makeTransparent(serverScroll->viewport());



        // Test button: top-right above the server list
        const int testX = width() - m - testBtn->width();
        const int testY = groupY - testH - 6;
        testBtn->move(testX, testY);
        testBtn->show();
        testBtn->raise();
    } else {
        groupScroll->hide();
        serverScroll->hide();
        testBtn->hide();
    }
}
