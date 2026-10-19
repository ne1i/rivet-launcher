#pragma once

#include "Config.h"

#include <QWidget>

class QCheckBox;
class QLabel;
class QPushButton;
class QToolButton;

class GameDetailPage : public QWidget
{
    Q_OBJECT
public:
    explicit GameDetailPage(QWidget *parent = nullptr);

    void setGame(const Game &game);
    void setPlayTime(qint64 seconds);
    void setRunning(bool running);
    void setShortcuts(bool desktop, bool menu);
    void setLaunchBusy(bool busy);
    void setStopping(bool stopping);
    const Game &game() const { return m_game; }

signals:
    void backRequested();
    void playRequested(const QString &gameName);
    void stopRequested();
    void settingsRequested(const QString &gameName);
    void debugRequested(const QString &gameName);
    void removeRequested(const QString &gameName);
    void utilityRequested(const QString &gameName, const QString &utility);
    void openFolderRequested(const QString &path);
    void runExeRequested(const QString &gameName);
    void shortcutToggled(const QString &gameName, bool desktop, bool enabled);
    void linkRequested(const QString &url);
    void logRequested(const QString &gameName);

private:
    void rebuildInfo();
    void updatePlayButton();
    void applyIcon();

    Game m_game;
    qint64 m_playTime = 0;
    bool m_running = false;
    bool m_busy = false;
    bool m_stopping = false;

    QLabel *m_icon = nullptr;
    QLabel *m_titleLabel = nullptr;
    QPushButton *m_playButton = nullptr;
    QPushButton *m_settingsButton = nullptr;
    QToolButton *m_utilityButton = nullptr;
    QToolButton *m_folderButton = nullptr;
    QPushButton *m_debugButton = nullptr;
    QPushButton *m_runButton = nullptr;
    QPushButton *m_removeButton = nullptr;
    QPushButton *m_steamButton = nullptr;
    QPushButton *m_protonDbButton = nullptr;
    QPushButton *m_steamDbButton = nullptr;
    QPushButton *m_logButton = nullptr;
    QCheckBox *m_desktopShortcut = nullptr;
    QCheckBox *m_menuShortcut = nullptr;
    QWidget *m_infoHost = nullptr;
    QLabel *m_meta = nullptr;
};
