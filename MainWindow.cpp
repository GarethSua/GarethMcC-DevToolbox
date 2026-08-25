#include "MainWindow.hpp"
#include "PasswordGenerator.hpp"
#include "HashCalculator.hpp"
#include "JsonFormatter.hpp"
#include "DuplicateFileFinder.hpp"
#include "TextSearcher.hpp"
#include "LogViewer.hpp"

#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QFileDialog>
#include <QTableWidgetItem>
#include <QtConcurrentRun>
#include <QDir>
#include <QProcess>
#include <QHeaderView>

#include <memory>
#include <atomic>
#include <cstdint>


// Converts raw bytes into a more readable file size
[[nodiscard]]
QString formatFileSize(const std::uintmax_t bytes)
{
    constexpr std::uintmax_t kilobyte{ 1024 };
    constexpr std::uintmax_t megabyte{ kilobyte * 1024 };
    constexpr std::uintmax_t gigabyte{ megabyte * 1024 };
    constexpr std::uintmax_t terabyte{ gigabyte * 1024 };

    if (bytes >= terabyte)
    {
        return QStringLiteral("%1 TB")
            .arg(static_cast<double>(bytes) / terabyte, 0, 'f', 2);
    }

    if (bytes >= gigabyte)
    {
        return QStringLiteral("%1 GB")
            .arg(static_cast<double>(bytes) / gigabyte, 0, 'f', 2);
    }

    if (bytes >= megabyte)
    {
        return QStringLiteral("%1 MB")
            .arg(static_cast<double>(bytes) / megabyte, 0, 'f', 2);
    }

    if (bytes >= kilobyte)
    {
        return QStringLiteral("%1 KB")
            .arg(static_cast<double>(bytes) / kilobyte, 0, 'f', 2);
    }

    return QStringLiteral("%1 B").arg(bytes);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    folderSynchroniseWatcher_ =
        std::make_unique<QFutureWatcher<FolderSynchroniseResult>>();

    folderSyncWatcher_ =
        std::make_unique<QFutureWatcher<FolderSyncResult>>();

    ui.logResultsTable->horizontalHeader()
        ->setSectionResizeMode(
            0,
            QHeaderView::ResizeToContents
        );

    ui.logResultsTable->horizontalHeader()
        ->setSectionResizeMode(
            1,
            QHeaderView::ResizeToContents
        );

    ui.logResultsTable->horizontalHeader()
        ->setSectionResizeMode(
            2,
            QHeaderView::Stretch
        );

    // adding a little space between of the navigation items
    ui.navigationList->setSpacing(4);

    // for Text Search table to fit the columns
    ui.textSearchResultsTable->horizontalHeader()
        ->setSectionResizeMode(0, QHeaderView::Stretch);

    ui.textSearchResultsTable->horizontalHeader()
        ->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    ui.textSearchResultsTable->horizontalHeader()
        ->setSectionResizeMode(2, QHeaderView::Stretch);


    // Folder Synchroniser table to fit the columns
    ui.folderSyncResultsTable->horizontalHeader()
        ->setSectionResizeMode(0, QHeaderView::Stretch);

    ui.folderSyncResultsTable->horizontalHeader()
        ->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    ui.folderSyncResultsTable->horizontalHeader()
        ->setSectionResizeMode(2, QHeaderView::ResizeToContents);


    // Duplicate File Finder table, get the point :)
    ui.duplicateResultsTable->horizontalHeader()
        ->setSectionResizeMode(0, QHeaderView::Stretch);

    ui.duplicateResultsTable->horizontalHeader()
        ->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    ui.duplicateResultsTable->horizontalHeader()
        ->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    textSearchWatcher_ =
        std::make_unique<QFutureWatcher<TextSearchResult>>();

    duplicateSearchWatcher_ =
        std::make_unique<QFutureWatcher<DuplicateSearchResult>>();

    connect(
        ui.navigationList,
        &QListWidget::currentRowChanged,
        this,
        [this](const int row)
        {
            switch (row)
            {
            case 0:
                ui.toolPages->setCurrentWidget(ui.duplicateFinderPage);
                break;

            case 1:
                ui.toolPages->setCurrentWidget(ui.passwordGeneratorPage);
                break;

            case 2:
                ui.toolPages->setCurrentWidget(ui.hashCalculatorPage);
                break;

            case 3:
                ui.toolPages->setCurrentWidget(ui.systemMonitorPage);
                break;

            case 4:
                ui.toolPages->setCurrentWidget(ui.folderSynchroniserPage);
                break;

            case 5:
                ui.toolPages->setCurrentWidget(ui.jsonFormatterPage);
                break;

            case 6:
                ui.toolPages->setCurrentWidget(ui.textSearchPage);
                break;

            case 7:
                ui.toolPages->setCurrentWidget(ui.logViewerPage);
                break;

            default:
                break;
            }
        }
    );

    connect(
        ui.generatePasswordButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const PasswordOptions options{
                .length = static_cast<std::size_t>(
                    ui.passwordLengthSpinBox->value()
                ),
                .includeUppercase = ui.uppercaseCheckBox->isChecked(),
                .includeLowercase = ui.lowercaseCheckBox->isChecked(),
                .includeNumbers = ui.numbersCheckBox->isChecked(),
                .includeSymbols = ui.symbolsCheckBox->isChecked()
            };

            const auto password = PasswordGenerator::generate(options);

            if (!password.has_value())
            {
                QMessageBox::warning(
                    this,
                    QStringLiteral("Cannot Generate Password"),
                    QStringLiteral(
                        "Select at least one character type."
                    )
                );

                return;
            }

            ui.generatedPasswordEdit->setText(
                QString::fromStdString(*password)
            );

            statusBar()->showMessage(
                QStringLiteral("Password generated"),
                2000
            );
        }
    );

    connect(
        ui.copyPasswordButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const auto password = ui.generatedPasswordEdit->text();

            if (password.isEmpty())
            {
                statusBar()->showMessage(
                    QStringLiteral("Generate a password first"),
                    2000
                );
                return;
            }

            QApplication::clipboard()->setText(password);

            statusBar()->showMessage(
                QStringLiteral("Password copied to clipboard"),
                2000
            );
        }
    );

    connect(
        ui.copyPasswordButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const auto password = ui.generatedPasswordEdit->text();

            if (password.isEmpty())
            {
                statusBar()->showMessage(
                    QStringLiteral("Generate a password first"),
                    2000
                );

                return;
            }

            QApplication::clipboard()->setText(password);

            statusBar()->showMessage(
                QStringLiteral("Password copied to clipboard"),
                2000
            );
        }
    );

    ui.navigationList->setCurrentRow(0);
    ui.toolPages->setCurrentWidget(ui.duplicateFinderPage);

    connect(
        ui.browseHashFileButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const auto filePath = QFileDialog::getOpenFileName(
                this,
                QStringLiteral("Select File")
            );

            if (filePath.isEmpty())
            {
                return;
            }

            ui.hashFilePathEdit->setText(filePath);
            ui.hashResultEdit->clear();

            statusBar()->showMessage(
                QStringLiteral("File selected"),
                2000
            );
        }
    );

    connect(
        ui.calculateHashButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const auto filePath = ui.hashFilePathEdit->text();

            if (filePath.isEmpty())
            {
                QMessageBox::warning(
                    this,
                    QStringLiteral("No File Selected"),
                    QStringLiteral("Please select a file first.")
                );
                return;
            }

            HashAlgorithm algorithm{ HashAlgorithm::Sha256 };

            switch (ui.hashAlgorithmComboBox->currentIndex())
            {
            case 0:
                algorithm = HashAlgorithm::Sha256;
                break;

            case 1:
                algorithm = HashAlgorithm::Sha512;
                break;

            case 2:
                algorithm = HashAlgorithm::Md5;
                break;

            default:
                break;
            }

            const auto result =
                HashCalculator::calculate(filePath, algorithm);

            if (!result.succeeded())
            {
                QMessageBox::warning(
                    this,
                    QStringLiteral("Hash Calculation Failed"),
                    result.error
                );
                return;
            }

            ui.hashResultEdit->setText(result.hash);

            statusBar()->showMessage(
                QStringLiteral("Hash calculated"),
                2000
            );
        }
    );

    connect(
        ui.copyHashButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const auto hash = ui.hashResultEdit->text();

            if (hash.isEmpty())
            {
                statusBar()->showMessage(
                    QStringLiteral("Calculate a hash first"),
                    2000
                );
                return;
            }

            QApplication::clipboard()->setText(hash);

            statusBar()->showMessage(
                QStringLiteral("Hash copied to clipboard"),
                2000
            );
        }
    );

    connect(
        ui.formatJsonButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const auto input = ui.jsonInputEdit->toPlainText();
            const auto result = JsonFormatter::format(input);

            if (!result.succeeded())
            {
                QMessageBox::warning(
                    this,
                    QStringLiteral("Invalid JSON"),
                    result.error
                );
                return;
            }

            ui.jsonOutputEdit->setPlainText(result.output);

            statusBar()->showMessage(
                QStringLiteral("JSON formatted"),
                2000
            );
        }
    );

    connect(
        ui.minifyJsonButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const auto input = ui.jsonInputEdit->toPlainText();
            const auto result = JsonFormatter::minify(input);

            if (!result.succeeded())
            {
                QMessageBox::warning(
                    this,
                    QStringLiteral("Invalid JSON"),
                    result.error
                );
                return;
            }

            ui.jsonOutputEdit->setPlainText(result.output);

            statusBar()->showMessage(
                QStringLiteral("JSON minified"),
                2000
            );
        }
    );

    connect(
        ui.validateJsonButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const auto input = ui.jsonInputEdit->toPlainText();
            const auto result = JsonFormatter::validate(input);

            if (!result.succeeded())
            {
                QMessageBox::warning(
                    this,
                    QStringLiteral("Invalid JSON"),
                    result.error
                );
                return;
            }

            ui.jsonOutputEdit->setPlainText(result.output);

            statusBar()->showMessage(
                QStringLiteral("JSON is valid"),
                2000
            );
        }
    );

    connect(
        ui.clearJsonButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            ui.jsonInputEdit->clear();
            ui.jsonOutputEdit->clear();

            statusBar()->showMessage(
                QStringLiteral("JSON cleared"),
                2000
            );
        }
    );
       
    connect(
        ui.browseSearchFolderButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const auto folderPath = QFileDialog::getExistingDirectory(
                this,
                QStringLiteral("Select Folder")
            );

            if (folderPath.isEmpty())
            {
                return;
            }

            ui.searchFolderPathEdit->setText(folderPath);
            ui.textSearchResultsTable->setRowCount(0);
            ui.textSearchStatusLabel->setText(
                QStringLiteral("Ready")
            );
        }
    );

    connect(
        ui.searchFilesButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            if (textSearchWatcher_->isRunning())
            {
                return;
            }

            const TextSearchOptions options{
                .folderPath = ui.searchFolderPathEdit->text(),
                .query = ui.searchQueryEdit->text(),
                .fileTypes = ui.fileTypesEdit->text(),
                .recursive = ui.recursiveSearchCheckBox->isChecked(),
                .caseSensitive = ui.caseSensitiveCheckBox->isChecked()
            };

            ui.textSearchResultsTable->setRowCount(0);

            textSearchCancelFlag_ =
                std::make_shared<std::atomic_bool>(false);

            const auto cancelFlag = textSearchCancelFlag_;

            ui.searchFilesButton->setEnabled(false);
            ui.cancelTextSearchButton->setEnabled(true);

            ui.textSearchStatusLabel->setText(
                QStringLiteral("Searching...")
            );

            const auto future = QtConcurrent::run(
                [options, cancelFlag]()
                {
                    return TextSearcher::search(
                        options,
                        *cancelFlag
                    );
                }
            );

            textSearchWatcher_->setFuture(future);
        }
    );

    connect(
        ui.cancelTextSearchButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            if (!textSearchCancelFlag_)
            {
                return;
            }

            // Tell the background search to stop.
            textSearchCancelFlag_->store(
                true,
                std::memory_order_relaxed
            );

            ui.cancelTextSearchButton->setEnabled(false);

            ui.textSearchStatusLabel->setText(
                QStringLiteral("Cancelling...")
            );
        }
    );

    connect(
        textSearchWatcher_.get(),
        &QFutureWatcher<TextSearchResult>::finished,
        this,
        [this]()
        {
            const bool wasCancelled =
                textSearchCancelFlag_ &&
                textSearchCancelFlag_->load(
                    std::memory_order_relaxed
                );

            const auto result =
                textSearchWatcher_->result();

            ui.searchFilesButton->setEnabled(true);
            ui.cancelTextSearchButton->setEnabled(false);

            if (!result.succeeded())
            {
                QMessageBox::warning(
                    this,
                    QStringLiteral("Search Failed"),
                    result.error
                );

                ui.textSearchStatusLabel->setText(
                    QStringLiteral("Search failed")
                );

                textSearchCancelFlag_.reset();
                return;
            }

            // Put every match into the results table.
            for (const auto& match : result.matches)
            {
                const auto row =
                    ui.textSearchResultsTable->rowCount();

                ui.textSearchResultsTable->insertRow(row);

                auto fileItem =
                    std::make_unique<QTableWidgetItem>(
                        match.filePath
                    );

                auto lineItem =
                    std::make_unique<QTableWidgetItem>(
                        QString::number(match.lineNumber)
                    );

                auto matchItem =
                    std::make_unique<QTableWidgetItem>(
                        match.lineText
                    );

                ui.textSearchResultsTable->setItem(
                    row,
                    0,
                    fileItem.release()
                );

                ui.textSearchResultsTable->setItem(
                    row,
                    1,
                    lineItem.release()
                );

                ui.textSearchResultsTable->setItem(
                    row,
                    2,
                    matchItem.release()
                );
            }

            if (wasCancelled)
            {
                ui.textSearchStatusLabel->setText(
                    QStringLiteral("Search cancelled - %1 matches found")
                    .arg(result.matches.size())
                );
            }
            else
            {
                ui.textSearchStatusLabel->setText(
                    QStringLiteral("%1 matches found")
                    .arg(result.matches.size())
                );
            }

            textSearchCancelFlag_.reset();
        }


    );

    connect(
        ui.browseDuplicateFolderButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const auto folderPath = QFileDialog::getExistingDirectory(
                this,
                QStringLiteral("Select Folder")
            );

            if (folderPath.isEmpty())
            {
                return;
            }

            ui.duplicateFolderPathEdit->setText(folderPath);
            ui.duplicateResultsTable->setRowCount(0);
            ui.duplicateStatusLabel->setText(
                QStringLiteral("Ready")
            );
        }
    );

    connect(
        ui.findDuplicatesButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            // this don't allow two duplicate scans at the same time
            if (duplicateSearchWatcher_->isRunning())
            {
                return;
            }

            //Will read the options chosen in the ui
            const DuplicateSearchOptions options{
                .folderPath = ui.duplicateFolderPathEdit->text(),
                .recursive = ui.duplicateRecursiveCheckBox->isChecked()
            };

            // Clear results left over from the previous scan.
            ui.duplicateResultsTable->setRowCount(0);

            // Fresh cancellation flag for this scan.
            duplicateSearchCancelFlag_ =
                std::make_shared<std::atomic_bool>(false);

            const auto cancelFlag =
                duplicateSearchCancelFlag_;

            ui.findDuplicatesButton->setEnabled(false);
            ui.cancelDuplicateSearchButton->setEnabled(true);

            ui.duplicateStatusLabel->setText(
                QStringLiteral("Scanning...")
            );

            // Run the filesystem work away from the ui thread.
            const auto future = QtConcurrent::run(
                [options, cancelFlag]()
                {
                    return DuplicateFileFinder::find(
                        options,
                        *cancelFlag
                    );
                }
            );

            duplicateSearchWatcher_->setFuture(future);
        }
    );

    connect(
        ui.cancelDuplicateSearchButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            if (!duplicateSearchCancelFlag_)
            {
                return;
            }

            // im telling the background duplicate scan to stop
            duplicateSearchCancelFlag_->store(
                true,
                std::memory_order_relaxed
            );

            ui.cancelDuplicateSearchButton->setEnabled(false);

            ui.duplicateStatusLabel->setText(
                QStringLiteral("Cancelling...")
            );
        }
    );

    connect(
        duplicateSearchWatcher_.get(),
        &QFutureWatcher<DuplicateSearchResult>::finished,
        this,
        [this]()
        {
            const auto result =
                duplicateSearchWatcher_->result();

            // reseting the buttons now that the scan has finished
            ui.findDuplicatesButton->setEnabled(true);
            ui.cancelDuplicateSearchButton->setEnabled(false);

            if (!result.succeeded())
            {
                QMessageBox::warning(
                    this,
                    QStringLiteral("Duplicate Scan Failed"),
                    result.error
                );

                ui.duplicateStatusLabel->setText(
                    QStringLiteral("Scan failed")
                );

                duplicateSearchCancelFlag_.reset();
                return;
            }

            int groupNumber{ 1 };

            // Add each duplicate file to the results table.
            for (const auto& group : result.groups)
            {
                for (const auto& filePath : group.filePaths)
                {
                    const auto row =
                        ui.duplicateResultsTable->rowCount();

                    ui.duplicateResultsTable->insertRow(row);

                    auto fileItem =
                        std::make_unique<QTableWidgetItem>(
                            filePath
                        );

                    auto sizeItem =
                        std::make_unique<QTableWidgetItem>(
                            formatFileSize(group.sizeBytes)
                        );

                    auto groupItem =
                        std::make_unique<QTableWidgetItem>(
                            QStringLiteral("Group %1")
                            .arg(groupNumber)
                        );

                    ui.duplicateResultsTable->setItem(
                        row,
                        0,
                        fileItem.release()
                    );

                    ui.duplicateResultsTable->setItem(
                        row,
                        1,
                        sizeItem.release()
                    );

                    ui.duplicateResultsTable->setItem(
                        row,
                        2,
                        groupItem.release()
                    );
                }

                ++groupNumber;
            }

            if (result.cancelled)
            {
                ui.duplicateStatusLabel->setText(
                    QStringLiteral("Scan cancelled")
                );
            }
            else
            {
                ui.duplicateStatusLabel->setText(
                    QStringLiteral("%1 duplicate groups found")
                    .arg(result.groups.size())
                );
            }

            duplicateSearchCancelFlag_.reset();
        }
    );

    connect(
        ui.duplicateResultsTable,
        &QTableWidget::itemSelectionChanged,
        this,
        [this]()
        {
                ui.showDuplicateInFolderButton->setEnabled(
                ui.duplicateResultsTable->currentRow() >= 0
            );
        }
    );

    connect(
        ui.showDuplicateInFolderButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const auto row =
                ui.duplicateResultsTable->currentRow();

            if (row < 0)
            {
                return;
            }

            const auto* fileItem =
                ui.duplicateResultsTable->item(row, 0);

            if (fileItem == nullptr)
            {
                return;
            }

            const auto filePath = fileItem->text();

            QProcess::startDetached(
                QStringLiteral("explorer.exe"),
                {
                    QStringLiteral("/select,"),
                    QDir::toNativeSeparators(filePath)
                }
            );
        }
    );

    connect(
        ui.browseLogFileButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const auto filePath = QFileDialog::getOpenFileName(
                this,
                QStringLiteral("Select Log File"),
                {},
                QStringLiteral(
                    "Log Files (*.log *.txt);;All Files (*.*)"
                )
            );

            if (filePath.isEmpty())
            {
                return;
            }

            ui.logFilePathEdit->setText(filePath);
            ui.logResultsTable->setRowCount(0);

            ui.logStatusLabel->setText(
                QStringLiteral("Ready")
            );
        }
    );

    connect(
        ui.loadLogButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            auto result =
                LogViewer::load(
                    ui.logFilePathEdit->text()
                );

            if (!result.succeeded())
            {
                QMessageBox::warning(
                    this,
                    QStringLiteral("Log Load Failed"),
                    result.error
                );

                ui.logStatusLabel->setText(
                    QStringLiteral("Load failed")
                );

                return;
            }

            loadedLogEntries_ = std::move(result.entries);

            refreshLogTable();
        }
    );

    connect(
        ui.logFilterEdit,
        &QLineEdit::textChanged,
        this,
        [this]()
        {
            refreshLogTable();
        }
    );

    connect(
        ui.logLevelComboBox,
        &QComboBox::currentIndexChanged,
        this,
        [this](int)
        {
            refreshLogTable();
        }
    );

    connect(
        ui.clearLogButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            loadedLogEntries_.clear();

            ui.logResultsTable->setRowCount(0);
            ui.logFilePathEdit->clear();
            ui.logFilterEdit->clear();
            ui.logLevelComboBox->setCurrentIndex(0);

            ui.logStatusLabel->setText(
                QStringLiteral("Ready")
            );
        }
    );

    // Manual refresh button
    connect(
        ui.refreshSystemMonitorButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            refreshSystemMonitor();
        }
    );

    // this Will automatically refresh the system information every second
    connect(
        &systemMonitorTimer_,
        &QTimer::timeout,
        this,
        [this]()
        {
            refreshSystemMonitor();
        }
    );

    systemMonitorTimer_.start(1000);

    refreshSystemMonitor();

    connect(
        ui.browseSourceFolderButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const auto folderPath =
                QFileDialog::getExistingDirectory(
                    this,
                    QStringLiteral("Select Source Folder")
                );

            if (folderPath.isEmpty())
            {
                return;
            }

            ui.sourceFolderPathEdit->setText(folderPath);

            // Clear any old comparison results.
            ui.folderSyncResultsTable->setRowCount(0);
            ui.synchroniseFoldersButton->setEnabled(false);

            ui.folderSyncStatusLabel->setText(
                QStringLiteral("Ready")
            );
        }
    );

    connect(
        ui.browseDestinationFolderButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const auto folderPath =
                QFileDialog::getExistingDirectory(
                    this,
                    QStringLiteral("Select Destination Folder")
                );

            if (folderPath.isEmpty())
            {
                return;
            }

            ui.destinationFolderPathEdit->setText(folderPath);

            // Clear any old comparison results.
            ui.folderSyncResultsTable->setRowCount(0);
            ui.synchroniseFoldersButton->setEnabled(false);

            ui.folderSyncStatusLabel->setText(
                QStringLiteral("Ready")
            );
        }
    );

    connect(
        ui.compareFoldersButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            // Don't start another comparison while one is already running.
            if (folderSyncWatcher_->isRunning())
            {
                return;
            }

            const FolderSyncOptions options{
                .sourceFolder = ui.sourceFolderPathEdit->text(),
                .destinationFolder = ui.destinationFolderPathEdit->text(),
                .recursive = ui.syncRecursiveCheckBox->isChecked()
            };

            // Remove results from the previous comparison.
            ui.folderSyncResultsTable->setRowCount(0);

            // Don't allow Synchronise until a fresh comparison succeeds.
            ui.synchroniseFoldersButton->setEnabled(false);

            // Create a fresh thread-safe Cancel flag.
            folderSyncCancelFlag_ =
                std::make_shared<std::atomic_bool>(false);

            const auto cancelFlag =
                folderSyncCancelFlag_;

            ui.compareFoldersButton->setEnabled(false);
            ui.cancelFolderSyncButton->setEnabled(true);

            ui.folderSyncStatusLabel->setText(
                QStringLiteral("Comparing...")
            );

            // Compare the folders on a background thread.
            const auto future = QtConcurrent::run(
                [options, cancelFlag]()
                {
                    return FolderSynchroniser::compare(
                        options,
                        *cancelFlag
                    );
                }
            );

            folderSyncWatcher_->setFuture(future);
        }
    );

    connect(
        ui.cancelFolderSyncButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            if (!folderSyncCancelFlag_)
            {
                return;
            }

            // Tell the background comparison to stop.
            folderSyncCancelFlag_->store(
                true,
                std::memory_order_relaxed
            );

            ui.cancelFolderSyncButton->setEnabled(false);

            ui.folderSyncStatusLabel->setText(
                QStringLiteral("Cancelling...")
            );
        }
    );

    connect(
        folderSyncWatcher_.get(),
        &QFutureWatcher<FolderSyncResult>::finished,
        this,
        [this]()
        {
            const auto result =
                folderSyncWatcher_->result();

            ui.compareFoldersButton->setEnabled(true);
            ui.cancelFolderSyncButton->setEnabled(false);

            if (!result.succeeded())
            {
                QMessageBox::warning(
                    this,
                    QStringLiteral("Folder Comparison Failed"),
                    result.error
                );

                ui.folderSyncStatusLabel->setText(
                    QStringLiteral("Comparison failed")
                );

                folderSyncCancelFlag_.reset();
                return;
            }

            // Keep the approved comparison plan for the Synchronise step.
            folderSyncItems_ = result.items;

            folderSyncOptions_ = FolderSyncOptions{
                .sourceFolder = ui.sourceFolderPathEdit->text(),
                .destinationFolder = ui.destinationFolderPathEdit->text(),
                .recursive = ui.syncRecursiveCheckBox->isChecked()
            };

            // Display each comparison result.
            for (const auto& item : result.items)
            {
                const auto row =
                    ui.folderSyncResultsTable->rowCount();

                ui.folderSyncResultsTable->insertRow(row);

                auto fileItem =
                    std::make_unique<QTableWidgetItem>(
                        item.relativePath
                    );

                auto statusItem =
                    std::make_unique<QTableWidgetItem>(
                        FolderSynchroniser::statusToString(
                            item.status
                        )
                    );

                auto actionItem =
                    std::make_unique<QTableWidgetItem>(
                        FolderSynchroniser::actionToString(
                            item.action
                        )
                    );

                ui.folderSyncResultsTable->setItem(
                    row,
                    0,
                    fileItem.release()
                );

                ui.folderSyncResultsTable->setItem(
                    row,
                    1,
                    statusItem.release()
                );

                ui.folderSyncResultsTable->setItem(
                    row,
                    2,
                    actionItem.release()
                );
            }

            if (result.cancelled)
            {
                ui.folderSyncStatusLabel->setText(
                    QStringLiteral("Comparison cancelled")
                );

                ui.synchroniseFoldersButton->setEnabled(false);
            }
            else
            {
                ui.folderSyncStatusLabel->setText(
                    QStringLiteral("%1 files compared")
                    .arg(result.items.size())
                );

                // We'll use this button in the next stage.
                ui.synchroniseFoldersButton->setEnabled(true);
            }

            folderSyncCancelFlag_.reset();
        }
    );

    connect(
        ui.synchroniseFoldersButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            if (folderSynchroniseWatcher_->isRunning())
            {
                return;
            }

            std::size_t copyCount{ 0 };
            std::size_t updateCount{ 0 };

            for (const auto& item : folderSyncItems_)
            {
                if (item.action == SyncAction::Copy)
                {
                    ++copyCount;
                }
                else if (item.action == SyncAction::Update)
                {
                    ++updateCount;
                }
            }

            const auto answer = QMessageBox::question(
                this,
                QStringLiteral("Confirm Synchronisation"),
                QStringLiteral(
                    "This will copy %1 new file(s) and overwrite %2 changed file(s).\n\nContinue?"
                )
                .arg(copyCount)
                .arg(updateCount),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
            );

            if (answer != QMessageBox::Yes)
            {
                return;
            }

            folderSyncCancelFlag_ =
                std::make_shared<std::atomic_bool>(false);

            const auto cancelFlag =
                folderSyncCancelFlag_;

            const auto options =
                folderSyncOptions_;

            const auto items =
                folderSyncItems_;

            ui.compareFoldersButton->setEnabled(false);
            ui.synchroniseFoldersButton->setEnabled(false);
            ui.cancelFolderSyncButton->setEnabled(true);

            ui.folderSyncStatusLabel->setText(
                QStringLiteral("Synchronising...")
            );

            const auto future = QtConcurrent::run(
                [options, items, cancelFlag]()
                {
                    return FolderSynchroniser::synchronise(
                        options,
                        items,
                        *cancelFlag
                    );
                }
            );

            folderSynchroniseWatcher_->setFuture(future);
        }
    );

    connect(
        folderSynchroniseWatcher_.get(),
        &QFutureWatcher<FolderSynchroniseResult>::finished,
        this,
        [this]()
        {
            const auto result =
                folderSynchroniseWatcher_->result();

            ui.compareFoldersButton->setEnabled(true);
            ui.cancelFolderSyncButton->setEnabled(false);

            if (!result.succeeded())
            {
                QMessageBox::warning(
                    this,
                    QStringLiteral("Synchronisation Failed"),
                    result.error
                );

                ui.folderSyncStatusLabel->setText(
                    QStringLiteral("Synchronisation failed")
                );

                folderSyncCancelFlag_.reset();
                return;
            }

            if (result.cancelled)
            {
                ui.folderSyncStatusLabel->setText(
                    QStringLiteral(
                        "Synchronisation cancelled - %1 copied, %2 updated"
                    )
                    .arg(result.copiedFiles)
                    .arg(result.updatedFiles)
                );
            }
            else
            {
                ui.folderSyncStatusLabel->setText(
                    QStringLiteral(
                        "Synchronisation complete - %1 copied, %2 updated"
                    )
                    .arg(result.copiedFiles)
                    .arg(result.updatedFiles)
                );
            }

            // Force a fresh comparison before another synchronisation.
            ui.synchroniseFoldersButton->setEnabled(false);

            folderSyncItems_.clear();
            folderSyncCancelFlag_.reset();
        }
    );

}

void MainWindow::refreshLogTable()
{
    // remove whatever is currently displayed
    ui.logResultsTable->setRowCount(0);

    const auto filterText =
        ui.logFilterEdit->text();

    const auto selectedLevel =
        ui.logLevelComboBox->currentIndex();

    std::size_t shownCount{ 0 };

    // Go through every log line previously loaded.
    for (const auto& entry : loadedLogEntries_)
    {
        // Skip messages that do not contain the filter text.
        if (!filterText.isEmpty() &&
            !entry.message.contains(
                filterText,
                Qt::CaseInsensitive))
        {
            continue;
        }

        bool levelMatches{ true };

        // 0 = All, 1 = Info, 2 = Warning, 3 = Error.
        switch (selectedLevel)
        {
        case 1:
            levelMatches =
                entry.level == LogLevel::Info;
            break;

        case 2:
            levelMatches =
                entry.level == LogLevel::Warning;
            break;

        case 3:
            levelMatches =
                entry.level == LogLevel::Error;
            break;

        default:
            break;
        }

        if (!levelMatches)
        {
            continue;
        }

        const auto row =
            ui.logResultsTable->rowCount();

        ui.logResultsTable->insertRow(row);

        auto lineItem =
            std::make_unique<QTableWidgetItem>(
                QString::number(entry.lineNumber)
            );

        auto levelItem =
            std::make_unique<QTableWidgetItem>(
                LogViewer::levelToString(entry.level)
            );

        auto messageItem =
            std::make_unique<QTableWidgetItem>(
                entry.message
            );

        ui.logResultsTable->setItem(
            row,
            0,
            lineItem.release()
        );

        ui.logResultsTable->setItem(
            row,
            1,
            levelItem.release()
        );

        ui.logResultsTable->setItem(
            row,
            2,
            messageItem.release()
        );

        ++shownCount;
    }

    ui.logStatusLabel->setText(
        QStringLiteral("%1 of %2 lines shown")
        .arg(shownCount)
        .arg(loadedLogEntries_.size())
    );
}

void MainWindow::refreshSystemMonitor()
{
    const auto stats = systemMonitor_.sample();

    // Display the CPU name.
    ui.cpuNameValueLabel->setText(
        stats.cpuName.isEmpty()
        ? QStringLiteral("Unknown")
        : stats.cpuName
    );

    // Display the number of logical processors.
    ui.processorCountValueLabel->setText(
        QString::number(stats.logicalProcessorCount)
    );

    // Display the GPU name(s).
    QStringList gpuNames;

    for (const auto& gpuName : stats.gpuNames)
    {
        gpuNames.push_back(gpuName);
    }

    ui.gpuNameValueLabel->setText(
        gpuNames.isEmpty()
        ? QStringLiteral("Unavailable")
        : gpuNames.join(QStringLiteral(" / "))
    );

    // CPU
    if (stats.cpuUsagePercent.has_value())
    {
        const auto cpuPercent =
            static_cast<int>(*stats.cpuUsagePercent);

        ui.cpuUsageProgressBar->setValue(cpuPercent);

        ui.cpuUsageValueLabel->setText(
            QStringLiteral("%1%")
            .arg(cpuPercent)
        );
    }
    else
    {
        ui.cpuUsageValueLabel->setText(
            QStringLiteral("Sampling...")
        );
    }

    // Memory
    const auto memoryPercent =
        static_cast<int>(stats.memoryUsagePercent);

    ui.memoryUsageProgressBar->setValue(
        memoryPercent
    );

    ui.memoryUsageValueLabel->setText(
        QStringLiteral("%1 / %2")
        .arg(formatFileSize(stats.usedMemoryBytes))
        .arg(formatFileSize(stats.totalMemoryBytes))
    );

    // Uptime
    const auto totalMinutes =
        stats.uptimeSeconds / 60;

    const auto days =
        totalMinutes / (60 * 24);

    const auto hours =
        (totalMinutes / 60) % 24;

    const auto minutes =
        totalMinutes % 60;

    ui.uptimeValueLabel->setText(
        QStringLiteral("%1d %2h %3m")
        .arg(days)
        .arg(hours)
        .arg(minutes)
    );
}



MainWindow::~MainWindow()
{}

