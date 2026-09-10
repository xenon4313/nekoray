#pragma once

#include <QDialog>
#include <QStringList>
#include <QJsonArray>

class QListWidget;
class QLineEdit;
class QPlainTextEdit;
class QTabWidget;
class QComboBox;

// Friendly editor for Custom Route JSON (routing->custom + custom_route_global).
// Supports direct/proxy lists plus per-server (p-<id>) app/site split routing.
class SimpleRouteEditor : public QDialog {
    Q_OBJECT

public:
    explicit SimpleRouteEditor(QWidget *parent = nullptr);

    void loadFromJson(const QString &json);
    [[nodiscard]] QString toJson() const;

    static bool addRuleToCustomRoute(const QString &matcher, bool isApp, bool isProxy);

private:
    struct ListPage {
        QListWidget *list = nullptr;
        QLineEdit *edit = nullptr;
    };

    struct ServerMapPage {
        QListWidget *list = nullptr;
        QLineEdit *edit = nullptr;
        QComboBox *server = nullptr;
    };

    ListPage makeDomainPage(QWidget *parent, const QString &placeholder, ListPage *opposing = nullptr);
    ListPage makeAppPage(QWidget *parent, const QString &placeholder, ListPage *opposing = nullptr);
    ServerMapPage makeServerDomainPage(QWidget *parent);
    ServerMapPage makeServerAppPage(QWidget *parent);

    void refillServerCombo(QComboBox *box);
    void addServerMapRow(QListWidget *list, const QString &matcher, int profileId);
    static void removeSelected(QListWidget *list);
    static void addListRow(QListWidget *list, const QString &text);
    static void removeMatching(QListWidget *list, const QString &text);
    static QJsonArray collectList(QListWidget *list);
    static QString profileOutboundTag(int profileId);
    static int profileIdFromOutboundTag(const QString &tag);
    QString displayNameForProfile(int profileId) const;

    void syncJsonFromLists();
    bool syncListsFromJson();
    void parseRulesIntoLists(const QJsonArray &rules);
    void clearFriendlyLists();

    QTabWidget *tabs = nullptr;
    ListPage directSites;
    ListPage proxySites;
    ListPage directApps;
    ListPage proxyApps;
    ServerMapPage serverSites;
    ServerMapPage serverApps;
    QPlainTextEdit *jsonEdit = nullptr;
    QStringList otherRulesJson;
    int lastTabIndex = 0;
    bool syncing = false;
};
