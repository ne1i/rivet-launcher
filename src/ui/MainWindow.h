#pragma once

#include <QMainWindow>
#include <QVector>

class AddGameWizard;
class FlowLayout;
class GameCard;
class GameDetailPage;
class Config;
class LogWindow;
class ProtonManager;
class QDialog;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QSystemTrayIcon;
class QWidget;
class Runner;
class Toast;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(Config *store, ProtonManager *protons, Runner *runner,
               bool minimalMode, const QString &initialLaunch, QWidget *parent = nullptr);

    void handleIpcCommand(const QString &command);
    void showWindow();
    bool minimalMode() const { return m_minimalMode; }

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildUi();
    QWidget *buildLibraryPage();
    void setupTray();
    void reloadLibrary();
    void refreshCard(const QString &gameName);
    void updateLibraryVisibility();
    void showLibrary();
    void showDetail(const QString &gameName);
    void playGame(const QString &gameName, const QString &debug = QString());
    void stopGame();
    void openAddGame(const QString &initialPath = QString());
    void openDownloadGames();
    void openLauncherSettings();
    void openGameSettings(const QString &gameName);
    void openRemoveGame(const QString &gameName);
    void openLog(const QString &gameName);
    void openUtilities(const QString &gameName, const QString &utility);
    void runExeInPrefix(const QString &gameName);
    void toggleShortcut(const QString &gameName, bool desktop, bool enabled);
    void applyFullscreen(bool enabled);
    void updateTrayState();
    void showToast(const QString &message, int duration = 3500);
    void updateStatusBar();
    void refreshDetailData();

    Config *m_store;
    ProtonManager *m_protons;
    Runner *m_runner;

    bool m_minimalMode = false;
    QString m_initialLaunch;
    QString m_currentLaunch;
    QString m_lastGame;

    QStackedWidget *m_pages = nullptr;
    QWidget *m_libraryPage = nullptr;
    QWidget *m_gridHost = nullptr;
    QWidget *m_emptyState = nullptr;
    FlowLayout *m_grid = nullptr;
    QLabel *m_libraryCount = nullptr;
    QLabel *m_emptyTitle = nullptr;
    QLabel *m_emptyHint = nullptr;
    QPushButton *m_addButton = nullptr;
    QPushButton *m_backButton = nullptr;
    QLineEdit *m_search = nullptr;
    GameDetailPage *m_detail = nullptr;
    Toast *m_toast = nullptr;
    QLabel *m_statusLeft = nullptr;
    QLabel *m_statusRight = nullptr;

    QVector<GameCard *> m_cards;

    QSystemTrayIcon *m_tray = nullptr;
    QDialog *m_steamDialog = nullptr;
    LogWindow *m_logWindow = nullptr;
};
