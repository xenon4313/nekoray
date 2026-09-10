#pragma once

#include <QWidget>
#include <QJsonArray>
#include <QTreeWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>

class DetailsPanel : public QWidget {
    Q_OBJECT

public:
    explicit DetailsPanel(QWidget *parent = nullptr);

    void updateConnections(const QJsonArray &arr);

private slots:
    void rebuildTree();
    void onContextMenu(const QPoint &pos);
    void addRuleAndPrompt(const QString &matcher, bool isApp, bool isProxy);

private:
    enum GroupMode {
        ByProcessHostIp = 0,
        ByHostIp = 1,
        ByProcessIp = 2,
    };

    enum SortMode {
        SortByName = 0,
        SortByLast = 1,
        SortByTraffic = 2,
    };

    QLineEdit *searchEdit = nullptr;
    QComboBox *groupCombo = nullptr;
    QComboBox *sortCombo = nullptr;
    QCheckBox *activeOnly = nullptr;
    QTreeWidget *tree = nullptr;
    QLabel *summary = nullptr;
    QJsonArray lastConnections;
};
