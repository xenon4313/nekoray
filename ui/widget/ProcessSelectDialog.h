#pragma once

#include <QDialog>
#include <QStringList>
#include <QList>
#include <QIcon>

class QTableWidget;
class QLineEdit;
class QPushButton;
class QLabel;

struct ProcessEntry {
    QString exeName;
    QString fullPath;
    QString windowTitle;
    int count = 1;
    QIcon icon;
};

class ProcessSelectDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProcessSelectDialog(QWidget *parent = nullptr);

    [[nodiscard]] QStringList selectedProcessNames() const;

private slots:
    void onRefresh();
    void onFilterChanged(const QString &text);
    void onItemDoubleClicked();
    void updateButtons();

private:
    void populateTable();
    static QList<ProcessEntry> enumerateRunningProcesses();

    QLineEdit *filterEdit = nullptr;
    QTableWidget *table = nullptr;
    QLabel *statusLabel = nullptr;
    QPushButton *refreshBtn = nullptr;
    QPushButton *addBtn = nullptr;
    QPushButton *cancelBtn = nullptr;

    QList<ProcessEntry> allProcesses;
    QStringList chosenProcesses;
};
