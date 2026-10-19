#include "Paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRandomGenerator>
#include <QStandardPaths>

namespace {

QString xdgUserDir(const QString &key, const QString &fallback)
{
    QProcess process;
    process.start(QStringLiteral("xdg-user-dir"), {key});
    if (process.waitForFinished(1500) && process.exitStatus() == QProcess::NormalExit
        && process.exitCode() == 0) {
        const QString value = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        if (!value.isEmpty() && value != QDir::homePath())
            return value;
    }
    return QDir::homePath() + QLatin1Char('/') + fallback;
}

QString configHome()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (path.isEmpty())
        path = QDir::homePath() + QStringLiteral("/.config");
    return path + QStringLiteral("/rivet");
}

QString dataHome()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (path.isEmpty())
        path = QDir::homePath() + QStringLiteral("/.local/share");
    return path + QStringLiteral("/rivet");
}

QString uniqueFile(const QString &directory, const QString &extension)
{
    Paths::ensureDir(directory);
    const QString suffix = extension.isEmpty() ? QString() : QLatin1Char('.') + extension;
    for (;;) {
        QString name(16, Qt::Uninitialized);
        static const char alphabet[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        for (int i = 0; i < name.size(); ++i)
            name[i] = QLatin1Char(alphabet[QRandomGenerator::global()->bounded(int(sizeof(alphabet) - 1))]);
        const QString candidate = directory + QLatin1Char('/') + name + suffix;
        if (!QFileInfo::exists(candidate))
            return candidate;
    }
}

} // namespace

namespace Paths {

QString configDir() { return configHome(); }
QString configFile() { return configHome() + QStringLiteral("/config.json"); }
QString dataDir() { return dataHome(); }
QString iconsDir() { return dataHome() + QStringLiteral("/icons"); }
QString coversDir() { return dataHome() + QStringLiteral("/covers"); }

QString downloadsDir() { return xdgUserDir(QStringLiteral("DOWNLOAD"), QStringLiteral("Downloads")); }
QString installsDir() { return QDir::homePath() + QStringLiteral("/Games"); }
QString defaultProtonsDir() { return dataHome() + QStringLiteral("/protons"); }
QString defaultPrefixesDir() { return dataHome() + QStringLiteral("/prefixes"); }

QString applicationsDir()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (path.isEmpty())
        path = QDir::homePath() + QStringLiteral("/.local/share/applications");
    return path;
}

QString desktopDir() { return xdgUserDir(QStringLiteral("DESKTOP"), QStringLiteral("Desktop")); }
QString documentsDir() { return xdgUserDir(QStringLiteral("DOCUMENTS"), QStringLiteral("Documents")); }

QString appExecutable()
{
    const QString path = QCoreApplication::applicationFilePath();
    return QFileInfo(path).absoluteFilePath();
}

QString bundledFile(const QString &name)
{
    return QStringLiteral(":/assets/") + name;
}

QString uniqueIconPath(const QString &extension) { return uniqueFile(iconsDir(), extension); }
QString uniqueCoverPath(const QString &extension) { return uniqueFile(coversDir(), extension); }

void ensureDir(const QString &path)
{
    if (!path.isEmpty())
        QDir().mkpath(path);
}

void ensureConfigDirs()
{
    ensureDir(configDir());
    ensureDir(dataDir());
    ensureDir(iconsDir());
    ensureDir(coversDir());
    ensureDir(applicationsDir());
}

} // namespace Paths
