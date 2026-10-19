#include "RarExtractor.h"

#include "Utils.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

QString RarExtractor::resolveArchive(const QString &archive, QString *error)
{
    if (QFileInfo(archive).isFile())
        return archive;

    // The caller may have selected "game.rar" while only "game.part1.rar"
    // exists. Pick the first part; unrar follows the remaining volumes.
    const QFileInfo info(archive);
    QDir dir = info.absoluteDir();
    QString base = info.completeBaseName();
    static const QRegularExpression partSuffix(QStringLiteral("\\.part\\d+$"),
                                                QRegularExpression::CaseInsensitiveOption);
    base.remove(partSuffix);

    const auto entries = dir.entryInfoList({base + QStringLiteral("*.rar")}, QDir::Files, QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (entry.fileName().contains(QLatin1String("part"), Qt::CaseInsensitive))
            return entry.absoluteFilePath();
    }
    if (!entries.isEmpty())
        return entries.first().absoluteFilePath();

    if (error)
        *error = QStringLiteral("Archive not found: %1").arg(archive);
    return {};
}

bool RarExtractor::available()
{
    return Utils::which(QStringLiteral("unrar"));
}

bool RarExtractor::run(const QStringList &arguments, QString *stdOut, QString *error,
                       const QString &password, bool *needsPassword)
{
    if (needsPassword)
        *needsPassword = false;
    if (!available()) {
        if (error)
            *error = QStringLiteral("unrar was not found. Install unrar with your package manager.");
        return false;
    }

    auto execute = [&](const QString &passwordArgument) {
        QStringList args;
        args << passwordArgument << QStringLiteral("-y") << arguments;
        return Utils::runProcess(QStringLiteral("unrar"), args, {},
                                 QProcessEnvironment::systemEnvironment(), 30 * 60 * 1000);
    };

    Utils::ProcessResult result = execute(password.isEmpty()
                                              ? QStringLiteral("-p-")
                                              : QStringLiteral("-p") + password);

    const bool wrongPassword = result.stdErr.contains(QLatin1String("Incorrect password"),
                                                       Qt::CaseInsensitive);
    if (wrongPassword && password.isEmpty()) {
        // OnlineFix archives use this well-known password.
        result = execute(QStringLiteral("-ponline-fix.me"));
    }

    if (result.stdErr.contains(QLatin1String("Incorrect password"), Qt::CaseInsensitive)) {
        if (needsPassword)
            *needsPassword = true;
        if (error)
            *error = QStringLiteral("Incorrect password for the archive");
        return false;
    }

    if (result.timedOut) {
        if (error)
            *error = QStringLiteral("unrar timed out");
        return false;
    }

    if (result.failedToStart) {
        if (error)
            *error = result.error;
        return false;
    }

    if (!result.ok()) {
        if (error) {
            QString message = result.stdErr.trimmed();
            if (message.isEmpty())
                message = QStringLiteral("unrar exited with code %1").arg(result.exitCode);
            *error = message;
        }
        return false;
    }

    if (stdOut)
        *stdOut = result.stdOut;
    return true;
}

bool RarExtractor::list(const QString &archive, QStringList *files, QString *error,
                        const QString &password, bool *needsPassword)
{
    const QString resolved = resolveArchive(archive, error);
    if (resolved.isEmpty())
        return false;

    QString output;
    if (!run({QStringLiteral("lb"), resolved}, &output, error, password, needsPassword))
        return false;

    if (files) {
        files->clear();
        const QStringList lines = output.split(QLatin1Char('\n'));
        for (QString line : lines) {
            line = line.trimmed();
            if (!line.isEmpty())
                files->append(line);
        }
    }
    return true;
}

bool RarExtractor::extract(const QString &archive, const QString &destination, QString *error,
                           const QString &password, bool *needsPassword)
{
    const QString resolved = resolveArchive(archive, error);
    if (resolved.isEmpty())
        return false;

    if (!QDir().mkpath(destination)) {
        if (error)
            *error = QStringLiteral("Could not create %1").arg(destination);
        return false;
    }

    return run({QStringLiteral("x"), resolved, destination + QLatin1Char('/')},
               nullptr, error, password, needsPassword);
}
