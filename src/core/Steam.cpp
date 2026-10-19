#include "Steam.h"

#include "Utils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>

namespace {

QString readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

QString defaultInstallPath()
{
    const QString home = QDir::homePath();
    const QStringList candidates = {
        home + QStringLiteral("/.steam/steam"),
        home + QStringLiteral("/.local/share/Steam"),
        home + QStringLiteral("/.steam/root"),
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo(candidate + QStringLiteral("/steamapps")).isDir())
            return candidate;
    }
    return candidates.first();
}

} // namespace

namespace Steam {

QString installPath()
{
    const QString fromEnv = qEnvironmentVariable("STEAM_COMPAT_CLIENT_INSTALL_PATH");
    if (!fromEnv.isEmpty() && QFileInfo(fromEnv + QStringLiteral("/steamapps")).isDir())
        return fromEnv;
    return defaultInstallPath();
}

bool isInstalled() { return Utils::which(QStringLiteral("steam")); }

bool isRunning()
{
    const Utils::ProcessResult result =
        Utils::runProcess(QStringLiteral("pidof"), {QStringLiteral("steam")}, {}, {}, 3000);
    return result.exitCode == 0 && !result.stdOut.trimmed().isEmpty();
}

QString loginUsersPath()
{
    return installPath() + QStringLiteral("/config/loginusers.vdf");
}

qint64 loginUsersModified()
{
    const QFileInfo info(loginUsersPath());
    return info.exists() ? info.lastModified().toMSecsSinceEpoch() : -1;
}

QString overlayRenderer32()
{
    return installPath() + QStringLiteral("/ubuntu12_32/gameoverlayrenderer.so");
}

QString overlayRenderer64()
{
    return installPath() + QStringLiteral("/ubuntu12_64/gameoverlayrenderer.so");
}

QStringList libraryFolders()
{
    QStringList folders;
    const QStringList candidates = {
        installPath(),
        QDir::homePath() + QStringLiteral("/.steam/steam"),
        QDir::homePath() + QStringLiteral("/.local/share/Steam"),
    };
    for (const QString &candidate : candidates) {
        if (!folders.contains(candidate) && QFileInfo(candidate).isDir())
            folders.append(candidate);
    }

    const QString vdfPath = installPath() + QStringLiteral("/steamapps/libraryfolders.vdf");
    const QString vdf = readText(vdfPath);
    if (!vdf.isEmpty()) {
        static const QRegularExpression pathRegex(QStringLiteral("\"path\"\\s+\"([^\"]+)\""));
        auto it = pathRegex.globalMatch(vdf);
        while (it.hasNext()) {
            const QString path = it.next().captured(1);
            if (!path.isEmpty() && !folders.contains(path))
                folders.append(path);
        }
    }
    return folders;
}

QString findRuntime(const QString &protonPath)
{
    if (protonPath.isEmpty())
        return {};

    const QString version = readText(protonPath + QStringLiteral("/version"));
    static const QRegularExpression versionRegex(QStringLiteral("GE-Proton(\\d+)-"));
    const auto match = versionRegex.match(version);
    int major = match.hasMatch() ? match.captured(1).toInt() : 11;
    if (major <= 0)
        major = 11;

    int appId = 1391110;
    QString directory = QStringLiteral("SteamLinuxRuntime_soldier");

    const QString cpu = QSysInfo::currentCpuArchitecture();
    const bool arm = cpu == QLatin1String("arm64") || cpu == QLatin1String("aarch64")
        || QSysInfo::buildCpuArchitecture() == QLatin1String("arm64");

    if (major >= 11) {
        if (arm) {
            appId = 4185400;
            directory = QStringLiteral("SteamLinuxRuntime_4-arm64");
        } else {
            appId = 4183110;
            directory = QStringLiteral("SteamLinuxRuntime_4");
        }
    } else if (major >= 8) {
        appId = 1628350;
        directory = QStringLiteral("SteamLinuxRuntime_sniper");
    }

    Q_UNUSED(appId);

    const QStringList folders = libraryFolders();
    for (const QString &folder : folders) {
        const QString run = folder + QStringLiteral("/steamapps/common/") + directory
            + QStringLiteral("/run");
        if (QFileInfo(run).isFile())
            return run;
    }
    return {};
}

} // namespace Steam
