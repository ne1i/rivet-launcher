#include "Runner.h"

#include "Config.h"
#include "ProtonManager.h"
#include "Steam.h"
#include "Utils.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

namespace {
constexpr int kSteamTimeoutMs = 7 * 60 * 1000;
}

Runner::Runner(Config *store, ProtonManager *protons, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_protons(protons)
    , m_builder(store, protons)
{
    m_steamTimer.setInterval(1000);
    connect(&m_steamTimer, &QTimer::timeout, this, &Runner::pollSteam);

    m_wineserverTimeout.setSingleShot(true);
    m_wineserverTimeout.setInterval(2 * 60 * 1000);
    connect(&m_wineserverTimeout, &QTimer::timeout, this, [this] {
        if (m_wineserver) {
            m_wineserver->kill();
            m_wineserver->waitForFinished(1000);
            m_wineserver->deleteLater();
            m_wineserver = nullptr;
        }
        finishSession(m_pendingExitCode);
    });
}

Runner::~Runner()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->disconnect(this);
        m_process->setParent(nullptr);
        m_process = nullptr;
    }
    if (m_wineserver && m_wineserver->state() != QProcess::NotRunning) {
        m_wineserver->disconnect(this);
        m_wineserver->setParent(nullptr);
        m_wineserver = nullptr;
    }
}

bool Runner::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

bool Runner::launch(const Game &game, const QString &debugValue)
{
    if (isRunning()) {
        emit launchFailed(game.name, QStringLiteral("Another game is already running from this launcher."));
        return false;
    }
    if (m_steamPending) {
        emit launchFailed(game.name, QStringLiteral("Already waiting for Steam to start."));
        return false;
    }

    m_game = game;
    m_debugValue = debugValue;
    m_log.clear();
    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
    m_stopping = false;
    m_sessionActive = true;

    if (!m_store->noSteamRequest()) {
        if (!Steam::isInstalled()) {
            emit launchFailed(game.name,
                              QStringLiteral("Steam is not installed, or it is a Flatpak/Snap build.\n\n"
                                             "The launcher needs the native Steam package."));
            return false;
        }
        if (!Steam::isRunning()) {
            beginSteamWait(game, debugValue);
            return true;
        }
    }

    startProcess(game, debugValue);
    return true;
}

void Runner::beginSteamWait(const Game &game, const QString &debugValue)
{
    m_pendingGame = game;
    m_pendingDebug = debugValue;
    m_steamStartMtime = Steam::loginUsersModified();
    m_steamElapsedMs = 0;
    m_steamCancelled = false;
    m_steamPending = true;

    emit statusChanged(QStringLiteral("Starting Steam…"));
    emit steamWaitStarted(game.name);

    QString error;
    if (!QProcess::startDetached(QStringLiteral("steam"), {QStringLiteral("-silent")})) {
        m_steamPending = false;
        emit steamWaitFinished(false, QStringLiteral("Steam could not be started"));
        emit launchFailed(game.name, QStringLiteral("Steam could not be started."));
        return;
    }

    m_steamTimer.start();
}

void Runner::pollSteam()
{
    if (!m_steamPending)
        return;

    m_steamElapsedMs += m_steamTimer.interval();

    const qint64 modified = Steam::loginUsersModified();
    const bool changed = modified != m_steamStartMtime && modified > 0;
    if (changed) {
        m_steamTimer.stop();
        m_steamPending = false;
        emit steamWaitFinished(true, QString());
        emit statusChanged(QStringLiteral("Steam is ready"));
        startProcess(m_pendingGame, m_pendingDebug);
        return;
    }

    if (m_steamElapsedMs > 15000 && !Steam::isRunning()) {
        m_steamTimer.stop();
        m_steamPending = false;
        const QString message = QStringLiteral("Steam closed before signing in. Start Steam manually and try again.");
        emit steamWaitFinished(false, message);
        emit launchFailed(m_pendingGame.name, message);
        return;
    }

    if (m_steamElapsedMs >= kSteamTimeoutMs) {
        m_steamTimer.stop();
        m_steamPending = false;
        const QString message = QStringLiteral("Timed out waiting for Steam. Start Steam and sign in, then try again.");
        emit steamWaitFinished(false, message);
        emit launchFailed(m_pendingGame.name, message);
    }
}

void Runner::cancelSteamWait()
{
    if (!m_steamPending)
        return;
    m_steamPending = false;
    m_steamCancelled = true;
    m_steamTimer.stop();
    emit steamWaitFinished(false, QStringLiteral("Cancelled"));
    emit statusChanged(QStringLiteral("Launch cancelled"));
}

void Runner::startProcess(const Game &game, const QString &debugValue)
{
    const LaunchCommand command = m_builder.build(game, debugValue);
    if (!command.isValid()) {
        emit launchFailed(game.name, command.error);
        return;
    }

    for (const QString &warning : command.warnings)
        emit gameOutput(game.name, QStringLiteral("Warning: ") + warning, true);

    m_process = new QProcess(this);
    m_process->setProgram(command.program);
    m_process->setArguments(command.arguments);
    m_process->setWorkingDirectory(command.workingDirectory);
    m_process->setProcessEnvironment(command.environment);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::started, this, [this, game] {
        m_elapsed.start();
        emit statusChanged(QStringLiteral("Running %1").arg(game.name));
        emit gameStarted(game.name);
    });
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this] { readChannel(false); });
    connect(m_process, &QProcess::readyReadStandardError, this, [this] { readChannel(true); });
    connect(m_process, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus status) { handleFinished(exitCode, status); });
    connect(m_process, &QProcess::errorOccurred, this, [this, game](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            const QString message = QStringLiteral("Could not start the game process: %1")
                                        .arg(m_process ? m_process->errorString() : QStringLiteral("unknown error"));
            if (m_process) {
                m_process->deleteLater();
                m_process = nullptr;
            }
            emit statusChanged(QStringLiteral("Launch failed"));
            emit launchFailed(game.name, message);
        }
    });

    emit statusChanged(QStringLiteral("Launching %1…").arg(game.name));
    m_process->start();
}

void Runner::readChannel(bool stderrOutput)
{
    if (!m_process)
        return;
    const QByteArray data = stderrOutput ? m_process->readAllStandardError()
                                         : m_process->readAllStandardOutput();
    appendOutput(data, stderrOutput);
}

void Runner::appendOutput(const QByteArray &data, bool stderrOutput)
{
    QByteArray &buffer = stderrOutput ? m_stderrBuffer : m_stdoutBuffer;
    buffer.append(data);

    int newline = buffer.indexOf('\n');
    while (newline >= 0) {
        QByteArray line = buffer.left(newline);
        buffer.remove(0, newline + 1);
        if (line.endsWith('\r'))
            line.chop(1);
        const QString text = QString::fromUtf8(line);
        if (text.isEmpty())
            continue;
        m_log += (stderrOutput ? QStringLiteral("[err] ") : QStringLiteral("[out] ")) + text
            + QLatin1Char('\n');
        emit gameOutput(m_game.name, text, stderrOutput);
        newline = buffer.indexOf('\n');
    }
}

void Runner::handleFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_process)
        return;

    readChannel(false);
    readChannel(true);

    if (!m_stdoutBuffer.isEmpty()) {
        m_log += QStringLiteral("[out] ") + QString::fromUtf8(m_stdoutBuffer) + QLatin1Char('\n');
        emit gameOutput(m_game.name, QString::fromUtf8(m_stdoutBuffer), false);
        m_stdoutBuffer.clear();
    }
    if (!m_stderrBuffer.isEmpty()) {
        m_log += QStringLiteral("[err] ") + QString::fromUtf8(m_stderrBuffer) + QLatin1Char('\n');
        emit gameOutput(m_game.name, QString::fromUtf8(m_stderrBuffer), true);
        m_stderrBuffer.clear();
    }

    const qint64 seconds = m_elapsed.isValid() ? qMax<qint64>(0, m_elapsed.elapsed() / 1000) : 0;
    const int effectiveExit = status == QProcess::CrashExit ? -1 : exitCode;
    m_sessionSeconds = seconds;
    m_elapsed.invalidate();
    m_log += QStringLiteral("[rivet] Process exited with code %1 after %2 seconds\n")
                 .arg(effectiveExit)
                 .arg(seconds);

    m_process->deleteLater();
    m_process = nullptr;

    if (seconds > 0)
        m_store->addPlayTime(m_game.name, seconds);

    m_pendingExitCode = effectiveExit;
    emit statusChanged(QStringLiteral("Finishing %1…").arg(m_game.name));
    m_wineserverTimeout.start();
    waitForWineserver();
}

void Runner::waitForWineserver()
{
    const QString protonName = m_protons->resolve(m_game.proton);
    const QString wineserver = protonName.isEmpty()
        ? QString()
        : m_protons->executable(protonName, QStringLiteral("wineserver"));

    if (wineserver.isEmpty() || !QDir(m_game.winePrefixPath()).exists()) {
        finishSession(m_pendingExitCode);
        return;
    }

    m_wineserver = new QProcess(this);
    m_wineserver->setProgram(wineserver);
    m_wineserver->setArguments({QStringLiteral("-w")});
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("WINEPREFIX"), m_game.winePrefixPath());
    m_wineserver->setProcessEnvironment(environment);
    connect(m_wineserver, &QProcess::finished, this, [this] {
        m_wineserverTimeout.stop();
        if (m_wineserver) {
            m_wineserver->deleteLater();
            m_wineserver = nullptr;
        }
        finishSession(m_pendingExitCode);
    });
    connect(m_wineserver, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        m_wineserverTimeout.stop();
        if (m_wineserver) {
            m_wineserver->deleteLater();
            m_wineserver = nullptr;
        }
        finishSession(m_pendingExitCode);
    });
    m_wineserver->start();
}

void Runner::finishSession(int exitCode)
{
    if (!m_sessionActive)
        return;
    m_sessionActive = false;
    const QString name = m_game.name;
    m_wineserverTimeout.stop();
    m_elapsed.invalidate();
    m_debugValue.clear();
    m_stopping = false;
    emit statusChanged(QStringLiteral("Ready"));
    emit gameFinished(name, exitCode, m_sessionSeconds);
    m_sessionSeconds = 0;
}

void Runner::stop()
{
    if (!isRunning())
        return;

    m_stopping = true;
    emit statusChanged(QStringLiteral("Closing %1…").arg(m_game.name));

    const LaunchCommand command = m_builder.stop(m_game);
    if (command.isValid()) {
        auto *killer = new QProcess(this);
        killer->setProgram(command.program);
        killer->setArguments(command.arguments);
        killer->setProcessEnvironment(command.environment);
        connect(killer, &QProcess::finished, this, [this, killer](int exitCode) {
            if (exitCode != 0) {
                Utils::runProcess(
                    QStringLiteral("pkill"),
                    QStringList{QStringLiteral("-f"), QFileInfo(m_game.executable).fileName()},
                    {}, QProcessEnvironment::systemEnvironment(), 5000);
            }
            killer->deleteLater();
        });
        connect(killer, &QProcess::errorOccurred, this, [this, killer](QProcess::ProcessError) {
            Utils::runProcess(
                QStringLiteral("pkill"),
                QStringList{QStringLiteral("-f"), QFileInfo(m_game.executable).fileName()},
                {}, QProcessEnvironment::systemEnvironment(), 5000);
            killer->deleteLater();
        });
        killer->start();
    } else {
        Utils::runProcess(QStringLiteral("pkill"),
                          QStringList{QStringLiteral("-f"), QFileInfo(m_game.executable).fileName()},
                          {}, QProcessEnvironment::systemEnvironment(), 5000);
    }

    // If the prefix refuses to shut down, terminate the Proton process itself.
    QTimer::singleShot(15000, this, [this] {
        if (m_process && m_process->state() != QProcess::NotRunning) {
            m_process->terminate();
            QTimer::singleShot(5000, this, [this] {
                if (m_process && m_process->state() != QProcess::NotRunning)
                    m_process->kill();
            });
        }
    });
}
