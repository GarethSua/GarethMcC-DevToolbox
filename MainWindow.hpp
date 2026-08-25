#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_MainWindow.h"

#include "TextSearcher.hpp"
#include "DuplicateFileFinder.hpp"
#include "LogViewer.hpp"
#include "SystemMonitor.hpp"
#include "FolderSynchroniser.hpp"

#include <QFutureWatcher>
#include <QTimer>
#include <atomic>
#include <memory>
#include <vector>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindowClass ui;

    std::unique_ptr<QFutureWatcher<TextSearchResult>>
        textSearchWatcher_;

    std::shared_ptr<std::atomic_bool>
        textSearchCancelFlag_;

    std::unique_ptr<QFutureWatcher<DuplicateSearchResult>>
        duplicateSearchWatcher_;

    std::shared_ptr<std::atomic_bool>
        duplicateSearchCancelFlag_;

    std::vector<LogEntry> loadedLogEntries_;

    SystemMonitor systemMonitor_;
    QTimer systemMonitorTimer_;

    void refreshSystemMonitor();

    void refreshLogTable();

    std::unique_ptr<QFutureWatcher<FolderSyncResult>>
        folderSyncWatcher_;

    std::shared_ptr<std::atomic_bool>
        folderSyncCancelFlag_;

    std::vector<FolderSyncItem> folderSyncItems_;

    FolderSyncOptions folderSyncOptions_;

    std::unique_ptr<QFutureWatcher<FolderSynchroniseResult>>
        folderSynchroniseWatcher_;
};

