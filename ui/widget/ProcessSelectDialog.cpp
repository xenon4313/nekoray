#include "ProcessSelectDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QDir>
#include <QStyle>
#include <QSet>
#include <algorithm>
#include <vector>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#elif defined(Q_OS_LINUX)
#include <unistd.h>
#endif

namespace {
#ifdef Q_OS_WIN
struct WindowCollectData {
    QMap<DWORD, QString> pidToTitle;
};

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindow(hwnd, GW_OWNER) != NULL) return TRUE;

    int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return TRUE;

    auto *data = reinterpret_cast<WindowCollectData*>(lParam);
    if (!data->pidToTitle.contains(pid)) {
        std::vector<wchar_t> buf(len + 1);
        GetWindowTextW(hwnd, buf.data(), len + 1);
        QString title = QString::fromWCharArray(buf.data()).trimmed();
        if (!title.isEmpty()) {
            data->pidToTitle[pid] = title;
        }
    }
    return TRUE;
}
#endif
} // namespace

ProcessSelectDialog::ProcessSelectDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Running Applications"));
    resize(640, 520);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // Search and Refresh row
    auto *topRow = new QHBoxLayout;
    filterEdit = new QLineEdit(this);
    filterEdit->setPlaceholderText(tr("Search by name, window title, or path…"));
    filterEdit->setClearButtonEnabled(true);

    refreshBtn = new QPushButton(tr("Refresh"), this);
    refreshBtn->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));

    topRow->addWidget(filterEdit, 1);
    topRow->addWidget(refreshBtn);
    mainLayout->addLayout(topRow);

    // Table widget
    table = new QTableWidget(this);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({tr("Application"), tr("Window / Title"), tr("Instances")});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setAlternatingRowColors(true);
    table->setSortingEnabled(false); // we handle sorted population
    table->verticalHeader()->setVisible(false);
    table->setShowGrid(false);

    auto *header = table->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    mainLayout->addWidget(table, 1);

    // Bottom row: Status and Actions
    auto *bottomRow = new QHBoxLayout;
    statusLabel = new QLabel(this);
    statusLabel->setStyleSheet("color: #888;");

    addBtn = new QPushButton(tr("Add Selected"), this);
    addBtn->setDefault(true);
    addBtn->setEnabled(false);

    cancelBtn = new QPushButton(tr("Cancel"), this);

    bottomRow->addWidget(statusLabel);
    bottomRow->addStretch(1);
    bottomRow->addWidget(addBtn);
    bottomRow->addWidget(cancelBtn);
    mainLayout->addLayout(bottomRow);

    // Connect signals
    connect(refreshBtn, &QPushButton::clicked, this, &ProcessSelectDialog::onRefresh);
    connect(filterEdit, &QLineEdit::textChanged, this, &ProcessSelectDialog::onFilterChanged);
    connect(table, &QTableWidget::itemDoubleClicked, this, &ProcessSelectDialog::onItemDoubleClicked);
    connect(table, &QTableWidget::itemSelectionChanged, this, &ProcessSelectDialog::updateButtons);

    connect(addBtn, &QPushButton::clicked, this, [this] {
        chosenProcesses.clear();
        QSet<QString> uniqueNames;
        for (auto *item : table->selectedItems()) {
            if (item->column() == 0) {
                QString exe = item->data(Qt::UserRole).toString();
                if (!exe.isEmpty() && !uniqueNames.contains(exe)) {
                    uniqueNames.insert(exe);
                    chosenProcesses.append(exe);
                }
            }
        }
        if (!chosenProcesses.isEmpty()) {
            accept();
        }
    });

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    onRefresh();
}

QStringList ProcessSelectDialog::selectedProcessNames() const {
    return chosenProcesses;
}

void ProcessSelectDialog::onRefresh() {
    allProcesses = enumerateRunningProcesses();
    populateTable();
}

void ProcessSelectDialog::populateTable() {
    table->setRowCount(0);
    table->setRowCount(static_cast<int>(allProcesses.size()));

    QFileIconProvider defaultIconProvider;
    QIcon genericExeIcon = style()->standardIcon(QStyle::SP_FileIcon);

    for (int i = 0; i < allProcesses.size(); ++i) {
        const auto &entry = allProcesses.at(i);

        // Column 0: Executable name with icon
        auto *itemExe = new QTableWidgetItem(entry.exeName);
        itemExe->setData(Qt::UserRole, entry.exeName);
        itemExe->setToolTip(entry.fullPath.isEmpty() ? entry.exeName : entry.fullPath);
        if (!entry.icon.isNull()) {
            itemExe->setIcon(entry.icon);
        } else {
            itemExe->setIcon(genericExeIcon);
        }
        itemExe->setFlags(itemExe->flags() & ~Qt::ItemIsEditable);
        table->setItem(i, 0, itemExe);

        // Column 1: Window Title / Description
        auto *itemTitle = new QTableWidgetItem(entry.windowTitle);
        itemTitle->setToolTip(entry.windowTitle.isEmpty() ? entry.fullPath : entry.windowTitle);
        if (entry.windowTitle.isEmpty()) {
            itemTitle->setForeground(QBrush(QColor(130, 130, 130)));
        }
        itemTitle->setFlags(itemTitle->flags() & ~Qt::ItemIsEditable);
        table->setItem(i, 1, itemTitle);

        // Column 2: Instances count
        auto *itemCount = new QTableWidgetItem(QString::number(entry.count));
        itemCount->setTextAlignment(Qt::AlignCenter);
        itemCount->setFlags(itemCount->flags() & ~Qt::ItemIsEditable);
        table->setItem(i, 2, itemCount);
    }

    onFilterChanged(filterEdit->text());
}

void ProcessSelectDialog::onFilterChanged(const QString &text) {
    const auto filter = text.trimmed();
    int visibleCount = 0;

    for (int r = 0; r < table->rowCount(); ++r) {
        bool match = true;
        if (!filter.isEmpty()) {
            auto *itemExe = table->item(r, 0);
            auto *itemTitle = table->item(r, 1);
            QString exe = itemExe ? itemExe->data(Qt::UserRole).toString() : QString();
            QString title = itemTitle ? itemTitle->text() : QString();
            QString path = itemExe ? itemExe->toolTip() : QString();

            match = exe.contains(filter, Qt::CaseInsensitive) ||
                    title.contains(filter, Qt::CaseInsensitive) ||
                    path.contains(filter, Qt::CaseInsensitive);
        }
        table->setRowHidden(r, !match);
        if (match) visibleCount++;
    }

    if (statusLabel) {
        if (filter.isEmpty()) {
            statusLabel->setText(tr("%1 running applications").arg(table->rowCount()));
        } else {
            statusLabel->setText(tr("%1 of %2 applications").arg(visibleCount).arg(table->rowCount()));
        }
    }
    updateButtons();
}

void ProcessSelectDialog::onItemDoubleClicked() {
    int row = table->currentRow();
    if (row >= 0) {
        auto *item = table->item(row, 0);
        if (item) {
            QString exe = item->data(Qt::UserRole).toString();
            if (!exe.isEmpty()) {
                chosenProcesses = {exe};
                accept();
            }
        }
    }
}

void ProcessSelectDialog::updateButtons() {
    bool hasSelection = false;
    for (auto *item : table->selectedItems()) {
        if (item->column() == 0 && !table->isRowHidden(item->row())) {
            hasSelection = true;
            break;
        }
    }
    addBtn->setEnabled(hasSelection);
}

QList<ProcessEntry> ProcessSelectDialog::enumerateRunningProcesses() {
    QList<ProcessEntry> result;

#ifdef Q_OS_WIN
    WindowCollectData winData;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&winData));

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return result;
    }

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    struct TempGroup {
        QString exeName;
        QString fullPath;
        QString windowTitle;
        int count = 0;
    };
    QMap<QString, TempGroup> groups;

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (pe.th32ProcessID == 0 || pe.th32ProcessID == 4) continue;
            QString exe = QString::fromWCharArray(pe.szExeFile).trimmed();
            if (exe.isEmpty() || exe.compare(QStringLiteral("[System Process]"), Qt::CaseInsensitive) == 0) continue;

            QString exeKey = exe.toLower();
            auto &g = groups[exeKey];
            g.count++;
            if (g.exeName.isEmpty()) {
                g.exeName = exe;
            }

            // Check if this pid has a visible window title
            if (g.windowTitle.isEmpty() && winData.pidToTitle.contains(pe.th32ProcessID)) {
                g.windowTitle = winData.pidToTitle.value(pe.th32ProcessID);
            }

            // Retrieve full path if not yet retrieved
            if (g.fullPath.isEmpty()) {
                HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
                if (hProc) {
                    wchar_t pathBuf[MAX_PATH * 4] = {0};
                    DWORD pathLen = sizeof(pathBuf) / sizeof(pathBuf[0]);
                    if (QueryFullProcessImageNameW(hProc, 0, pathBuf, &pathLen)) {
                        g.fullPath = QString::fromWCharArray(pathBuf);
                    }
                    CloseHandle(hProc);
                }
            }
        } while (Process32NextW(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);

    QFileIconProvider iconProvider;
    for (const auto &g : groups) {
        ProcessEntry entry;
        entry.exeName = g.exeName;
        entry.fullPath = g.fullPath;
        entry.windowTitle = g.windowTitle;
        entry.count = g.count;

        if (!g.fullPath.isEmpty() && QFile::exists(g.fullPath)) {
            entry.icon = iconProvider.icon(QFileInfo(g.fullPath));
        }
        result.append(entry);
    }
#elif defined(Q_OS_LINUX)
    QDir procDir(QStringLiteral("/proc"));
    const auto entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QMap<QString, ProcessEntry> groups;

    for (const auto &name : entries) {
        bool ok = false;
        name.toLongLong(&ok);
        if (!ok) continue;

        QString commPath = QStringLiteral("/proc/%1/comm").arg(name);
        QFile commFile(commPath);
        if (commFile.open(QIODevice::ReadOnly)) {
            QString exe = QString::fromUtf8(commFile.readAll()).trimmed();
            if (!exe.isEmpty()) {
                QString key = exe.toLower();
                if (!groups.contains(key)) {
                    ProcessEntry pe;
                    pe.exeName = exe;
                    pe.count = 1;
                    char linkBuf[1024] = {0};
                    ssize_t len = readlink(QStringLiteral("/proc/%1/exe").arg(name).toUtf8().constData(), linkBuf, sizeof(linkBuf) - 1);
                    if (len > 0) {
                        pe.fullPath = QString::fromUtf8(linkBuf, len);
                    }
                    groups[key] = pe;
                } else {
                    groups[key].count++;
                }
            }
        }
    }
    for (const auto &pe : groups) result.append(pe);
#endif

    // Sort order:
    // 1. Applications with a visible window title come FIRST (alphabetical)
    // 2. Applications without a window title come SECOND (alphabetical)
    std::sort(result.begin(), result.end(), [](const ProcessEntry &a, const ProcessEntry &b) {
        bool aHasWin = !a.windowTitle.isEmpty();
        bool bHasWin = !b.windowTitle.isEmpty();
        if (aHasWin != bHasWin) {
            return aHasWin > bHasWin;
        }
        return a.exeName.compare(b.exeName, Qt::CaseInsensitive) < 0;
    });

    return result;
}
