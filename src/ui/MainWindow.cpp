#include "MainWindow.h"

#include "AddGameWizard.h"
#include "DesktopEntry.h"
#include "DownloadGamesDialog.h"
#include "FlowLayout.h"
#include "GameCard.h"
#include "GameDetailPage.h"
#include "GameSettingsDialog.h"
#include "Config.h"
#include "Icons.h"
#include "LauncherSettingsDialog.h"
#include "LogWindow.h"
#include "Paths.h"
#include "ProtonManager.h"
#include "RemoveGameDialog.h"
#include "Runner.h"
#include "Steam.h"
#include "Theme.h"
#include "Toast.h"
#include "Utils.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

MainWindow::MainWindow(Config *store, ProtonManager *protons, Runner *runner,
                       bool minimalMode, const QString &initialLaunch, QWidget *parent)
    : QMainWindow(parent)
    , m_store(store)
    , m_protons(protons)
    , m_runner(runner)
    , m_minimalMode(minimalMode)
    , m_initialLaunch(initialLaunch)
{
    setWindowTitle(QStringLiteral("Rivet"));
    setWindowIcon(Icons::appIcon());
    resize(1180, 760);
    setMinimumSize(920, 600);
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);

    buildUi();
    qApp->installEventFilter(this);
    setupTray();
    reloadLibrary();
    updateStatusBar();
    if (m_store->migratedFromLegacy()) {
        showToast(QStringLiteral("Imported %1 games into Rivet's new config.")
                      .arg(m_store->gameCount()),
                  5000);
    }

    connect(m_runner, &Runner::statusChanged, this, [this](const QString &message) {
        m_statusLeft->setText(message);
    });

    connect(m_runner, &Runner::steamWaitStarted, this, [this](const QString &gameName) {
        if (m_steamDialog)
            return;
        m_steamDialog = new QDialog(this);
        m_steamDialog->setWindowTitle(QStringLiteral("Waiting for Steam"));
        m_steamDialog->setModal(true);
        m_steamDialog->setMinimumWidth(430);
        auto *layout = new QVBoxLayout(m_steamDialog);
        layout->setContentsMargins(24, 20, 24, 16);
        layout->setSpacing(12);
        auto *label = new QLabel(
            QStringLiteral("Starting Steam and waiting for you to sign in…\n\n"
                           "The game will start automatically once Steam is ready."));
        label->setWordWrap(true);
        layout->addWidget(label);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::rejected, m_runner, &Runner::cancelSteamWait);
        connect(buttons, &QDialogButtonBox::rejected, m_steamDialog, &QDialog::reject);
        layout->addWidget(buttons);
        m_steamDialog->show();
        Q_UNUSED(gameName);
    });

    connect(m_runner, &Runner::steamWaitFinished, this, [this](bool success, const QString &error) {
        if (m_steamDialog) {
            m_steamDialog->close();
            m_steamDialog->deleteLater();
            m_steamDialog = nullptr;
        }
        if (!success && !error.isEmpty() && error != QLatin1String("Cancelled"))
            m_statusLeft->setText(error);
        if (!success)
            m_detail->setLaunchBusy(false);
        if (!success && m_minimalMode)
            QApplication::quit();
    });

    connect(m_runner, &Runner::gameStarted, this, [this](const QString &gameName) {
        m_currentLaunch.clear();
        if (m_steamDialog) {
            m_steamDialog->close();
            m_steamDialog->deleteLater();
            m_steamDialog = nullptr;
        }
        if (m_detail->game().name == gameName) {
            m_detail->setStopping(false);
            m_detail->setRunning(true);
            m_detail->setLaunchBusy(false);
        }
        if (m_runner->isDebugRun() || m_logWindow)
            openLog(gameName);
        m_statusLeft->setText(QStringLiteral("Playing %1").arg(gameName));
        updateTrayState();
        refreshDetailData();
    });

    connect(m_runner, &Runner::gameOutput, this,
            [this](const QString &gameName, const QString &line, bool stderrOutput) {
                if (m_logWindow && m_runner->currentGame() == gameName)
                    m_logWindow->appendLine(line, stderrOutput);
            });

    connect(m_runner, &Runner::gameFinished, this,
            [this](const QString &gameName, int exitCode, qint64 seconds) {
                Q_UNUSED(exitCode);
                Q_UNUSED(seconds);
                m_currentLaunch.clear();
                if (m_detail->game().name == gameName) {
                    m_detail->setStopping(false);
                    m_detail->setRunning(false);
                    m_detail->setLaunchBusy(false);
                }
                refreshCard(gameName);
                refreshDetailData();
                updateStatusBar();
                updateTrayState();
                if (m_minimalMode && !isVisible()) {
                    QApplication::quit();
                    return;
                }
                if (!isVisible() && m_tray) {
                    m_tray->showMessage(QStringLiteral("Rivet"),
                                        QStringLiteral("%1 has closed.").arg(gameName),
                                        QSystemTrayIcon::Information, 3000);
                }
                if (!isVisible() && !m_minimalMode)
                    QApplication::quit();
            });

    connect(m_runner, &Runner::launchFailed, this,
            [this](const QString &gameName, const QString &message) {
                m_currentLaunch.clear();
                if (m_steamDialog) {
                    m_steamDialog->close();
                    m_steamDialog->deleteLater();
                    m_steamDialog = nullptr;
                }
                m_detail->setLaunchBusy(false);
                m_detail->setRunning(false);
                m_detail->setStopping(false);
                m_statusLeft->setText(QStringLiteral("Launch failed"));
                updateTrayState();
                QMessageBox::critical(this, QStringLiteral("Could not start %1").arg(gameName),
                                      message);
                if (m_minimalMode)
                    QApplication::quit();
            });

    if (!m_initialLaunch.isEmpty()) {
        QTimer::singleShot(0, this, [this] { playGame(m_initialLaunch); });
    }
}

void MainWindow::buildUi()
{
    auto *root = new QWidget;
    root->setObjectName(QStringLiteral("page"));
    auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *topBar = new QFrame;
    topBar->setObjectName(QStringLiteral("topBar"));
    topBar->setFixedHeight(74);
    auto *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(22, 10, 22, 10);
    topLayout->setSpacing(14);

    m_backButton = new QPushButton;
    m_backButton->setObjectName(QStringLiteral("ghost"));
    m_backButton->setIcon(Icons::icon(Icons::Name::Back, Theme::TextMuted, 21));
    m_backButton->setIconSize(QSize(21, 21));
    m_backButton->setFixedSize(38, 38);
    m_backButton->setToolTip(QStringLiteral("Back to library (mouse Back)"));
    m_backButton->hide();
    connect(m_backButton, &QPushButton::clicked, this, &MainWindow::showLibrary);
    topLayout->addWidget(m_backButton);

    auto *logo = new QLabel;
    logo->setPixmap(Icons::appIcon().pixmap(40, 40));
    logo->setFixedSize(40, 40);
    topLayout->addWidget(logo);

    auto *titles = new QVBoxLayout;
    titles->setSpacing(0);
    auto *appTitle = new QLabel(QStringLiteral("Rivet"));
    appTitle->setObjectName(QStringLiteral("appTitle"));
    auto *appSubtitle = new QLabel(QStringLiteral("Linux Launcher"));
    appSubtitle->setObjectName(QStringLiteral("appSubtitle"));
    titles->addWidget(appTitle);
    titles->addWidget(appSubtitle);
    topLayout->addLayout(titles);

    topLayout->addSpacing(24);

    m_search = new QLineEdit;
    m_search->setPlaceholderText(QStringLiteral("Search your library…"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(Icons::icon(Icons::Name::Search, Theme::TextMuted, 16),
                        QLineEdit::LeadingPosition);
    m_search->setMaximumWidth(380);
    connect(m_search, &QLineEdit::textChanged, this, [this] { updateLibraryVisibility(); });
    topLayout->addWidget(m_search, 1);

    m_addButton = new QPushButton(QStringLiteral("Add game"));
    m_addButton->setObjectName(QStringLiteral("primary"));
    m_addButton->setIcon(Icons::icon(Icons::Name::Plus, QColor(0x1a, 0x12, 0x06), 17));
    m_addButton->setIconSize(QSize(17, 17));
    m_addButton->setMinimumHeight(38);
    connect(m_addButton, &QPushButton::clicked, this, [this] { openAddGame(); });
    topLayout->addWidget(m_addButton);

    auto *downloads = new QPushButton;
    downloads->setObjectName(QStringLiteral("ghost"));
    downloads->setIcon(Icons::icon(Icons::Name::Download, Theme::TextMuted, 21));
    downloads->setIconSize(QSize(21, 21));
    downloads->setToolTip(QStringLiteral("Download games from a Hydra source"));
    downloads->setFixedSize(38, 38);
    connect(downloads, &QPushButton::clicked, this, &MainWindow::openDownloadGames);
    topLayout->addWidget(downloads);

    auto *settings = new QPushButton;
    settings->setObjectName(QStringLiteral("ghost"));
    settings->setIcon(Icons::icon(Icons::Name::Settings, Theme::TextMuted, 21));
    settings->setIconSize(QSize(21, 21));
    settings->setToolTip(QStringLiteral("Launcher settings"));
    settings->setFixedSize(38, 38);
    connect(settings, &QPushButton::clicked, this, &MainWindow::openLauncherSettings);
    topLayout->addWidget(settings);

    layout->addWidget(topBar);

    m_pages = new QStackedWidget;
    m_libraryPage = buildLibraryPage();
    m_pages->addWidget(m_libraryPage);

    m_detail = new GameDetailPage;
    m_pages->addWidget(m_detail);
    connect(m_detail, &GameDetailPage::backRequested, this, &MainWindow::showLibrary);
    connect(m_detail, &GameDetailPage::playRequested, this,
            [this](const QString &name) { playGame(name); });
    connect(m_detail, &GameDetailPage::stopRequested, this, &MainWindow::stopGame);
    connect(m_detail, &GameDetailPage::settingsRequested, this, &MainWindow::openGameSettings);
    connect(m_detail, &GameDetailPage::debugRequested, this, [this](const QString &name) {
        bool ok = false;
        const QString debug = QInputDialog::getText(
            this, QStringLiteral("Debug %1").arg(name), QStringLiteral("WINEDEBUG value:"),
            QLineEdit::Normal, QStringLiteral("+err,+warn,+seh"), &ok);
        if (!ok)
            return;
        playGame(name, debug.trimmed().isEmpty() ? QStringLiteral("+err,+warn,+seh") : debug.trimmed());
    });
    connect(m_detail, &GameDetailPage::removeRequested, this, &MainWindow::openRemoveGame);
    connect(m_detail, &GameDetailPage::utilityRequested, this, &MainWindow::openUtilities);
    connect(m_detail, &GameDetailPage::runExeRequested, this, &MainWindow::runExeInPrefix);
    connect(m_detail, &GameDetailPage::openFolderRequested, this, [this](const QString &path) {
        if (QFileInfo(path).isDir())
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        else
            showToast(QStringLiteral("That folder does not exist yet."));
    });
    connect(m_detail, &GameDetailPage::shortcutToggled, this, &MainWindow::toggleShortcut);
    connect(m_detail, &GameDetailPage::linkRequested, this, [](const QString &url) {
        QDesktopServices::openUrl(QUrl(url));
    });
    connect(m_detail, &GameDetailPage::logRequested, this, &MainWindow::openLog);

    layout->addWidget(m_pages, 1);
    setCentralWidget(root);

    m_toast = new Toast(this);

    m_statusLeft = new QLabel(QStringLiteral("Ready"));
    m_statusRight = new QLabel;
    statusBar()->addWidget(m_statusLeft, 1);
    statusBar()->addPermanentWidget(m_statusRight);
    statusBar()->setSizeGripEnabled(false);
    statusBar()->setFixedHeight(28);
}

QWidget *MainWindow::buildLibraryPage()
{
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(30, 24, 30, 36);
    layout->setSpacing(18);

    auto *headingHost = new QWidget;
    headingHost->setFixedHeight(48);
    auto *heading = new QHBoxLayout(headingHost);
    heading->setContentsMargins(0, 0, 0, 0);
    heading->setSpacing(12);
    auto *title = new QLabel(QStringLiteral("Your library"));
    title->setObjectName(QStringLiteral("pageTitle"));
    title->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    heading->addWidget(title, 0, Qt::AlignVCenter);
    m_libraryCount = new QLabel;
    m_libraryCount->setObjectName(QStringLiteral("badge"));
    m_libraryCount->setFixedHeight(28);
    m_libraryCount->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    heading->addWidget(m_libraryCount, 0, Qt::AlignVCenter);
    heading->addStretch();
    layout->addWidget(headingHost);

    m_gridHost = new QWidget;
    m_grid = new FlowLayout(m_gridHost, 0, 18, 18);
    layout->addWidget(m_gridHost);

    auto *empty = new QWidget;
    m_emptyState = empty;
    auto *emptyLayout = new QVBoxLayout(empty);
    emptyLayout->setContentsMargins(0, 70, 0, 0);
    emptyLayout->setSpacing(10);
    auto *emptyIcon = new QLabel;
    emptyIcon->setPixmap(Icons::pixmap(Icons::Name::Gamepad, Theme::TextMuted, 64));
    emptyIcon->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyIcon);
    m_emptyTitle = new QLabel(QStringLiteral("Your library is empty"));
    m_emptyTitle->setObjectName(QStringLiteral("pageTitle"));
    m_emptyTitle->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(m_emptyTitle);
    m_emptyHint = new QLabel(QStringLiteral(
        "Add a downloaded game folder, a RAR archive or an installer. "
        "You can also drag a folder onto this window."));
    m_emptyHint->setObjectName(QStringLiteral("muted"));
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_emptyHint->setWordWrap(true);
    emptyLayout->addWidget(m_emptyHint);
    auto *emptyButton = new QPushButton(QStringLiteral("Add your first game"));
    emptyButton->setObjectName(QStringLiteral("primary"));
    emptyButton->setIcon(Icons::icon(Icons::Name::Plus, QColor(0x1a, 0x12, 0x06), 17));
    connect(emptyButton, &QPushButton::clicked, this, [this] { openAddGame(); });
    emptyLayout->addWidget(emptyButton, 0, Qt::AlignHCenter);
    emptyLayout->addStretch();
    layout->addWidget(empty, 1);
    layout->addStretch();

    scroll->setWidget(content);
    return scroll;
}

void MainWindow::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_tray = new QSystemTrayIcon(Icons::appIcon(), this);
    m_tray->setToolTip(QStringLiteral("Rivet"));
    auto *menu = new QMenu(this);
    menu->addAction(QStringLiteral("Show launcher"), this, &MainWindow::showWindow);
    menu->addSeparator();
    menu->addAction(QStringLiteral("Quit"), this, [this] {
        if (m_runner->isRunning()) {
            showWindow();
            QMessageBox::information(this, QStringLiteral("Game running"),
                                     QStringLiteral("Close the game before quitting the launcher."));
            return;
        }
        QApplication::quit();
    });
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger
                    || reason == QSystemTrayIcon::DoubleClick)
                    showWindow();
            });
    m_tray->show();
}

void MainWindow::reloadLibrary()
{
    for (GameCard *card : m_cards) {
        card->hide();
        card->deleteLater();
    }
    m_cards.clear();

    while (QLayoutItem *item = m_grid->takeAt(0)) {
        if (item->widget()) {
            item->widget()->hide();
            item->widget()->deleteLater();
        }
        delete item;
    }

    const QStringList names = m_store->gameNames();
    for (const QString &name : names) {
        const Game game = m_store->game(name);
        auto *card = new GameCard(game);
        connect(card, &GameCard::clicked, this, &MainWindow::showDetail);
        m_grid->addWidget(card);
        m_cards.append(card);
    }

    updateLibraryVisibility();
    updateStatusBar();
}

void MainWindow::refreshCard(const QString &gameName)
{
    for (GameCard *card : m_cards) {
        if (card->game().name == gameName) {
            card->setGame(m_store->game(gameName));
            break;
        }
    }
}

void MainWindow::updateLibraryVisibility()
{
    const QString query = m_search ? m_search->text().trimmed() : QString();
    int visible = 0;
    for (GameCard *card : m_cards) {
        const bool match = query.isEmpty()
            || card->game().name.contains(query, Qt::CaseInsensitive);
        card->setVisible(match);
        if (match)
            ++visible;
    }
    if (m_grid)
        m_grid->invalidate();

    const int total = m_store->gameCount();
    m_libraryCount->setText(total == 1 ? QStringLiteral("1 game")
                                       : QStringLiteral("%1 games").arg(total));

    const bool showEmpty = visible == 0;
    if (m_gridHost)
        m_gridHost->setVisible(!showEmpty);
    if (m_emptyState)
        m_emptyState->setVisible(showEmpty);

    if (showEmpty) {
        if (total == 0) {
            m_emptyTitle->setText(QStringLiteral("Your library is empty"));
            m_emptyHint->setText(QStringLiteral(
                "Add a downloaded game folder, a RAR archive or an installer. "
                "You can also drag a folder onto this window."));
        } else {
            m_emptyTitle->setText(QStringLiteral("No games match “%1”").arg(query));
            m_emptyHint->setText(QStringLiteral("Try a different search, or clear the search field."));
        }
    }
}

void MainWindow::showLibrary()
{
    m_pages->setCurrentIndex(0);
    if (m_backButton)
        m_backButton->hide();
    setWindowTitle(QStringLiteral("Rivet"));
    m_search->clear();
    m_search->setFocus();
}

void MainWindow::showDetail(const QString &gameName)
{
    const Game game = m_store->game(gameName);
    if (!game.isValid()) {
        showToast(QStringLiteral("That game no longer exists."));
        return;
    }
    m_detail->setGame(game);
    m_detail->setStopping(false);
    m_detail->setShortcuts(DesktopEntry::exists(Paths::desktopDir(), gameName),
                           DesktopEntry::exists(Paths::applicationsDir(), gameName));
    m_detail->setRunning(m_runner->isRunning() && m_runner->currentGame() == gameName);
    m_detail->setLaunchBusy(m_currentLaunch == gameName && !m_runner->isRunning());
    m_lastGame = gameName;
    if (m_backButton)
        m_backButton->show();
    m_pages->setCurrentIndex(1);
    setWindowTitle(QStringLiteral("%1 — Rivet").arg(gameName));
}

void MainWindow::playGame(const QString &gameName, const QString &debug)
{
    const Game game = m_store->game(gameName);
    if (!game.isValid()) {
        QMessageBox::critical(this, QStringLiteral("Game not found"),
                              QStringLiteral("\"%1\" is not in the library.").arg(gameName));
        if (m_minimalMode)
            QApplication::quit();
        return;
    }

    if (m_runner->isRunning()) {
        if (m_runner->currentGame() == gameName)
            stopGame();
        else
            showToast(QStringLiteral("Another game is already running."));
        return;
    }

    m_currentLaunch = gameName;
    m_detail->setStopping(false);

    if (m_detail->game().name == gameName)
        m_detail->setLaunchBusy(true);

    if (!debug.isEmpty())
        openLog(gameName);

    m_runner->launch(game, debug);
}

void MainWindow::stopGame()
{
    if (!m_runner->isRunning())
        return;
    m_detail->setStopping(true);
    m_runner->stop();
}

void MainWindow::openAddGame(const QString &initialPath)
{
    QString path = initialPath;
    if (path.isEmpty()) {
        path = QFileDialog::getExistingDirectory(
            this, QStringLiteral("Select the game folder or the folder containing the download"),
            m_store->downloadsPath());
    }
    if (path.isEmpty())
        return;

    AddGameWizard wizard(m_store, m_protons, path, this);
    if (wizard.exec() != QDialog::Accepted)
        return;

    reloadLibrary();
    showDetail(wizard.addedGame().name);
    showToast(QStringLiteral("Added %1").arg(wizard.addedGame().name));
    for (const QString &message : wizard.resultMessages())
        qWarning("Add game: %s", qPrintable(message));
}

void MainWindow::openDownloadGames()
{
    DownloadGamesDialog dialog(m_store, m_protons, this);
    connect(&dialog, &DownloadGamesDialog::gameAdded, this, [this](const QString &name) {
        reloadLibrary();
        showDetail(name);
        showToast(QStringLiteral("Added %1").arg(name));
    });
    dialog.exec();
}

void MainWindow::openLauncherSettings()
{    LauncherSettingsDialog dialog(m_store, m_protons, this);
    connect(&dialog, &LauncherSettingsDialog::settingsSaved, this, [this] {
        applyFullscreen(m_store->fullscreen());
        updateStatusBar();
        reloadLibrary();
        showToast(QStringLiteral("Launcher settings saved."));
    });
    dialog.exec();
}

void MainWindow::openGameSettings(const QString &gameName)
{
    GameSettingsDialog dialog(m_store, m_protons, m_store->game(gameName), this);
    connect(&dialog, &GameSettingsDialog::saved, this, [this, &dialog] {
        reloadLibrary();
        showDetail(dialog.gameName());
        updateStatusBar();
        showToast(QStringLiteral("Game settings saved."));
    });
    dialog.exec();
}

void MainWindow::openRemoveGame(const QString &gameName)
{
    RemoveGameDialog dialog(m_store, m_store->game(gameName), this);
    connect(&dialog, &RemoveGameDialog::removed, this, [this](const QString &name) {
        reloadLibrary();
        showLibrary();
        showToast(QStringLiteral("Removed %1").arg(name));
    });
    dialog.exec();
}

void MainWindow::openLog(const QString &gameName)
{
    const QString text = m_runner->currentGame() == gameName ? m_runner->logText() : QString();
    if (!m_logWindow) {
        m_logWindow = new LogWindow(gameName, text, this);
        m_logWindow->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_logWindow, &QObject::destroyed, this, [this] { m_logWindow = nullptr; });
    } else {
        m_logWindow->setGameName(gameName);
        m_logWindow->setLog(text);
    }
    m_logWindow->show();
    m_logWindow->raise();
    m_logWindow->activateWindow();
}

void MainWindow::openUtilities(const QString &gameName, const QString &utility)
{
    const Game game = m_store->game(gameName);
    const QString protonName = m_protons->resolve(game.proton);
    if (protonName.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Proton"),
                             QStringLiteral("No Proton build is installed for this game."));
        return;
    }

    const QString wine = m_protons->executable(protonName, QStringLiteral("wine"));
    if (wine.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Proton"),
                             QStringLiteral("The wine binary is missing from this Proton build."));
        return;
    }

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("WINEPREFIX"), game.winePrefixPath());
    environment.insert(QStringLiteral("WINEDEBUG"), QStringLiteral("-all"));

    QString program = wine;
    QStringList arguments;
    if (utility == QLatin1String("winetricks")) {
        if (!Utils::which(QStringLiteral("winetricks"))) {
            QMessageBox::warning(this, QStringLiteral("Winetricks"),
                                 QStringLiteral("winetricks was not found in your PATH."));
            return;
        }
        program = QStringLiteral("winetricks");
        environment.insert(QStringLiteral("WINE"), wine);
    } else if (utility == QLatin1String("wineconsole")) {
        arguments = {QStringLiteral("start"), QStringLiteral("cmd.exe")};
    } else {
        arguments = {utility};
    }

    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setWorkingDirectory(game.workingDirectory());
    process.setProcessEnvironment(environment);
    if (!process.startDetached()) {
        QMessageBox::warning(this, QStringLiteral("Could not start %1").arg(utility),
                             QStringLiteral("Make sure the game prefix has been created by launching the game once."));
    }
}

void MainWindow::runExeInPrefix(const QString &gameName)
{
    const Game game = m_store->game(gameName);
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Run an executable in the game prefix"),
        game.workingDirectory(), QStringLiteral("Windows executables (*.exe *.bat *.cmd)"));
    if (path.isEmpty())
        return;

    const QString protonName = m_protons->resolve(game.proton);
    const QString proton = protonName.isEmpty()
        ? QString()
        : m_protons->executable(protonName, QStringLiteral("proton"));
    if (proton.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Proton"),
                             QStringLiteral("No Proton build is installed for this game."));
        return;
    }

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("STEAM_COMPAT_DATA_PATH"), game.effectivePrefixPath());
    environment.insert(QStringLiteral("STEAM_COMPAT_CLIENT_INSTALL_PATH"), Steam::installPath());
    environment.insert(QStringLiteral("WINEDEBUG"), QStringLiteral("-all"));

    QProcess process;
    process.setProgram(proton);
    process.setArguments({QStringLiteral("run"), path});
    process.setWorkingDirectory(QFileInfo(path).absolutePath());
    process.setProcessEnvironment(environment);
    if (!process.startDetached()) {
        QMessageBox::warning(this, QStringLiteral("Run .exe"),
                             QStringLiteral("Could not start the executable."));
    }
}

void MainWindow::toggleShortcut(const QString &gameName, bool desktop, bool enabled)
{
    const Game game = m_store->game(gameName);
    const QString directory = desktop ? Paths::desktopDir() : Paths::applicationsDir();
    QString error;
    bool ok = true;
    if (enabled)
        ok = DesktopEntry::create(directory, gameName, game.icon, &error);
    else
        ok = DesktopEntry::remove(directory, gameName);

    if (!ok)
        showToast(error.isEmpty() ? QStringLiteral("Could not update the shortcut.") : error);

    m_detail->setShortcuts(DesktopEntry::exists(Paths::desktopDir(), gameName),
                           DesktopEntry::exists(Paths::applicationsDir(), gameName));
}

void MainWindow::applyFullscreen(bool enabled)
{
    if (enabled) {
        if (isVisible())
            showFullScreen();
    } else if (isFullScreen()) {
        showNormal();
    }
}

void MainWindow::updateTrayState()
{
    if (!m_tray)
        return;
    if (m_runner->isRunning())
        m_tray->setToolTip(QStringLiteral("Rivet — %1 is running").arg(m_runner->currentGame()));
    else
        m_tray->setToolTip(QStringLiteral("Rivet"));
}

void MainWindow::showToast(const QString &message, int duration)
{
    if (m_toast)
        m_toast->showMessage(message, duration);
}

void MainWindow::updateStatusBar()
{
    if (!m_statusRight)
        return;
    const int count = m_store->gameCount();
    const QString proton = m_protons->latestInstalled();
    m_statusRight->setText(QStringLiteral("%1 game%2  •  %3  •  v%4")
                               .arg(count)
                               .arg(count == 1 ? QString() : QStringLiteral("s"))
                               .arg(proton.isEmpty() ? QStringLiteral("No Proton installed") : proton,
                                    QCoreApplication::applicationVersion()));
    if (!m_runner->isRunning() && m_statusLeft->text().isEmpty())
        m_statusLeft->setText(QStringLiteral("Ready"));
}

void MainWindow::refreshDetailData()
{
    if (m_pages->currentIndex() != 1)
        return;
    const QString name = m_detail->game().name;
    const Game game = m_store->game(name);
    if (!game.isValid())
        return;
    m_detail->setGame(game);
    m_detail->setRunning(m_runner->isRunning() && m_runner->currentGame() == name);
}

void MainWindow::showWindow()
{
    m_minimalMode = false;
    if (!isVisible()) {
        if (m_store->fullscreen())
            showFullScreen();
        else
            show();
    }
    if (isMinimized())
        showNormal();
    raise();
    activateWindow();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_runner->isRunning()) {
        hide();
        if (m_tray) {
            m_tray->showMessage(
                QStringLiteral("Rivet"),
                QStringLiteral("Still running in the background while %1 is open. "
                               "Start the launcher again to show this window.")
                    .arg(m_runner->currentGame()),
                QSystemTrayIcon::Information, 4000);
        }
        event->ignore();
        return;
    }
    event->accept();
    QApplication::quit();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress
        && !QApplication::activeModalWidget()) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::BackButton) {
            if (m_pages->currentIndex() == 1)
                showLibrary();
            return true;
        }
        if (mouse->button() == Qt::ForwardButton) {
            if (m_pages->currentIndex() == 0 && !m_lastGame.isEmpty())
                showDetail(m_lastGame);
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() & Qt::AltModifier) {
        if (event->key() == Qt::Key_Left) {
            if (m_pages->currentIndex() == 1)
                showLibrary();
            return;
        }
        if (event->key() == Qt::Key_Right) {
            if (m_pages->currentIndex() == 0 && !m_lastGame.isEmpty())
                showDetail(m_lastGame);
            return;
        }
    }
    if (event->key() == Qt::Key_Escape) {
        if (m_pages->currentIndex() == 1) {
            showLibrary();
            return;
        }
        if (m_search && m_search->hasFocus()) {
            m_search->clear();
            return;
        }
    } else if (event->key() == Qt::Key_F11) {
        const bool fullscreen = !isFullScreen();
        applyFullscreen(fullscreen);
        m_store->setFullscreen(fullscreen);
        m_store->save();
        return;
    } else if (event->matches(QKeySequence::Find)) {
        m_search->setFocus();
        m_search->selectAll();
        return;
    } else if (event->matches(QKeySequence::New)) {
        openAddGame();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty() && urls.first().isLocalFile())
            event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty() || !urls.first().isLocalFile())
        return;
    openAddGame(urls.first().toLocalFile());
}

void MainWindow::handleIpcCommand(const QString &command)
{
    if (command.startsWith(QLatin1String("LAUNCH\t"))) {
        const QString name = command.section(QLatin1Char('\t'), 1);
        showWindow();
        const Game game = m_store->game(name);
        if (!game.isValid()) {
            QMessageBox::critical(this, QStringLiteral("Game not found"),
                                  QStringLiteral("\"%1\" is not in the library.").arg(name));
            return;
        }
        showDetail(name);
        playGame(name);
    } else {
        showWindow();
    }
}
