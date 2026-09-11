#pragma once



#include <QWidget>
#include <QPixmap>
#include <QList>
#include <QVector>



class QPushButton;
class QLabel;
class QScrollArea;
class QVBoxLayout;
class QHBoxLayout;
class QFrame;
class TrafficSparkline;



class SimpleModeWidget : public QWidget {
    Q_OBJECT



public:
    explicit SimpleModeWidget(QWidget *parent = nullptr);



    void refreshServers();
    void refreshGroupTabs();
    void refreshPowerState();
    void reloadBackground();
    void updateServerLatency(int profileId = -1);
    void setUrlTestBusy(bool busy);
    void setDisconnecting(bool on);
    [[nodiscard]] bool isConnectionBusy() const;
    void pushTrafficSample(qint64 proxyUpRate, qint64 proxyDownRate,
                           qint64 directUpRate, qint64 directDownRate);



signals:
    void requestAdvancedMode();
    void requestToggleBackground();
    void requestPowerToggle(bool turnOn, int profileId);
    void requestEditRules();
    void requestCheckUpdate();
    void requestUrlTest();
    void requestPasteFromClipboard();
    void requestSelectGroup(int groupId);



protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;



private:
    void rebuildChromeLayout();
    QRect mapDesignRect(int x, int y, int w, int h) const;
    void updateHeroText();
    void fitHeroTitleFont();
    void updateSpeedLabels(qint64 proxyUpRate, qint64 proxyDownRate,
                           qint64 directUpRate, qint64 directDownRate);
    int selectedProfileId() const;
    void applyChromeButtonStyle(QPushButton *b);
    void applyServerCardStyle(QPushButton *card, bool checked);
    void applyLatencyLabel(QLabel *label, int latency);



    static constexpr int kDesignW = 824;
    static constexpr int kDesignH = 1280;
    static constexpr int kHeroX = 122;
    static constexpr int kHeroY = 470;
    static constexpr int kHeroW = 584;
    static constexpr int kHeroH = 140;
    static constexpr int kHistWindow = 60;
    static constexpr double kListHeightFraction = 0.37;



    QPixmap bg;
    QPushButton *advancedBtn = nullptr;
    QPushButton *updateBtn = nullptr;
    QPushButton *bgBtn = nullptr;
    QPushButton *rulesBtn = nullptr;
    QPushButton *testBtn = nullptr;



    QWidget *groupBar = nullptr;
    QHBoxLayout *groupBarLayout = nullptr;
    QScrollArea *groupScroll = nullptr;
    QList<QPushButton *> groupTabButtons;



    QLabel *speedProxy = nullptr;
    QLabel *speedDirect = nullptr;



    QFrame *hero = nullptr;
    TrafficSparkline *heroChart = nullptr;
    QLabel *heroTitle = nullptr;
    QLabel *heroSub = nullptr; // status under the hero block
    QLabel *heroLegend = nullptr;



    QScrollArea *serverScroll = nullptr;
    QWidget *serverListHost = nullptr;
    QVBoxLayout *serverListLayout = nullptr;
    QList<QPushButton *> serverCards;
    QList<QLabel *> serverLatencyLabels;
    int selectedId = -1;
    bool urlTestBusy = false;
    bool disconnecting = false;



    QVector<double> proxyUpHist;
    QVector<double> proxyDownHist;
    QVector<double> directUpHist;
    QVector<double> directDownHist;
};
