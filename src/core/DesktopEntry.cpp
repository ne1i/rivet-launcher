#include "DesktopEntry.h"

#include "Paths.h"
#include "Utils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace {

QString sanitizeName(QString name)
{
    name.replace(QLatin1Char('/'), QLatin1Char('-'));
    name.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return name;
}

} // namespace

namespace DesktopEntry {

QString fileNameFor(const QString &gameName)
{
    return sanitizeName(gameName) + QStringLiteral(".desktop");
}

QString filePath(const QString &directory, const QString &gameName)
{
    return directory + QLatin1Char('/') + fileNameFor(gameName);
}

bool exists(const QString &directory, const QString &gameName)
{
    return QFileInfo(filePath(directory, gameName)).isFile();
}

QString content(const QString &gameName, const QString &iconPath)
{
    const QString executable = Paths::appExecutable();
    QString result;
    result += QStringLiteral("[Desktop Entry]\n");
    result += QStringLiteral("Name=") + sanitizeName(gameName) + QLatin1Char('\n');
    result += QStringLiteral("GenericName=Play this game with Rivet\n");
    result += QStringLiteral("Comment=Launch ") + sanitizeName(gameName)
        + QStringLiteral(" with Rivet\n");
    result += QStringLiteral("Exec=") + Utils::desktopExecQuote(executable)
        + QStringLiteral(" --launch ") + Utils::desktopExecQuote(gameName) + QLatin1Char('\n');
    if (!iconPath.isEmpty())
        result += QStringLiteral("Icon=") + iconPath + QLatin1Char('\n');
    result += QStringLiteral("Path=") + QDir::homePath() + QLatin1Char('\n');
    result += QStringLiteral("Type=Application\n");
    result += QStringLiteral("Categories=Game;\n");
    result += QStringLiteral("Terminal=false\n");
    result += QStringLiteral("StartupNotify=false\n");
    return result;
}

bool create(const QString &directory, const QString &gameName, const QString &iconPath,
            QString *error)
{
    if (!QDir().mkpath(directory)) {
        if (error)
            *error = QStringLiteral("Could not create %1").arg(directory);
        return false;
    }

    QFile file(filePath(directory, gameName));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("Could not write %1: %2").arg(file.fileName(), file.errorString());
        return false;
    }
    const QByteArray data = content(gameName, iconPath).toUtf8();
    if (file.write(data) != data.size()) {
        if (error)
            *error = QStringLiteral("Could not write %1: %2").arg(file.fileName(), file.errorString());
        return false;
    }
    file.close();
    file.setPermissions(file.permissions() | QFileDevice::ExeOwner | QFileDevice::ReadOwner
                        | QFileDevice::WriteOwner);
    return true;
}

bool remove(const QString &directory, const QString &gameName)
{
    const QString path = filePath(directory, gameName);
    if (!QFileInfo(path).isFile())
        return true;
    return QFile::remove(path);
}

bool replaceIcon(const QString &directory, const QString &gameName, const QString &iconPath)
{
    const QString path = filePath(directory, gameName);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QString body = QString::fromUtf8(file.readAll());
    file.close();

    static const QRegularExpression iconLine(QStringLiteral("^Icon=.*$"),
                                               QRegularExpression::MultilineOption);
    body.remove(iconLine);
    body = QStringLiteral("Icon=") + iconPath + QLatin1Char('\n') + body;

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    file.write(body.toUtf8());
    return true;
}

} // namespace DesktopEntry
