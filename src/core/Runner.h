#pragma once

#include "CommandBuilder.h"
#include "Config.h"

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QTimer>

class Game;
class Config;
class ProtonManager;

class Runner : public QObject
{
    Q_OBJECT
public:
    Runner(Config *store, ProtonManager *protons, QObject *parent = nullptr);
    ~Runner() override;

    bool launch(const Game &game, const QString &debugValue = QString());
    bool isRunning() const;
    QString currentGame() const { return m_game.name; }
    bool isDebugRun() const { return !m_debugValue.isEmpty(); }
    QString logText() const { return m_log; }
    void stop();
    void cancelSteamWait();

signals:
    void statusChanged(const QString &message);
    void steamWaitStarted(const QString &gameName);
    void steamWaitFinished(bool success, const QString &error);
    void gameStarted(const QString &gameName);
    void gameOutput(const QString &gameName, const QString &line, bool stderrOutput);
    void gameFinished(const QString &gameName, int exitCode, qint64 seconds);
    void launchFailed(const QString &gameName, const QString &message);

private:
    void startProcess(const Game &game, const QString &debugValue);
    void beginSteamWait(const Game &game, const QString &debugValue);
    void pollSteam();
    void readChannel(bool stderrOutput);
    void handleFinished(int exitCode, QProcess::ExitStatus status);
    void appendOutput(const QByteArray &data, bool stderrOutput);
    void finishSession(int exitCode);
    void waitForWineserver();

    Config *m_store;
    ProtonManager *m_protons;
    CommandBuilder m_builder;

    QProcess *m_process = nullptr;
    Game m_game;
    QString m_debugValue;
    QElapsedTimer m_elapsed;
    QString m_log;
    QByteArray m_stdoutBuffer;
    QByteArray m_stderrBuffer;
    bool m_stopping = false;

    QTimer m_steamTimer;
    Game m_pendingGame;
    QString m_pendingDebug;
    qint64 m_steamStartMtime = -1;
    int m_steamElapsedMs = 0;
    bool m_steamPending = false;
    bool m_steamCancelled = false;

    QProcess *m_wineserver = nullptr;
    QTimer m_wineserverTimeout;
    int m_pendingExitCode = 0;
    qint64 m_sessionSeconds = 0;
    bool m_sessionActive = false;
};
