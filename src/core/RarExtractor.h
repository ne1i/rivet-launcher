#pragma once

#include <QString>
#include <QStringList>

class RarExtractor
{
public:
    static bool available();

    // Lists archive contents (relative paths). When needsPassword is set and
    // returns true, the caller should ask the user for a password and retry.
    static bool list(const QString &archive, QStringList *files, QString *error,
                     const QString &password = QString(), bool *needsPassword = nullptr);

    static bool extract(const QString &archive, const QString &destination, QString *error,
                        const QString &password = QString(), bool *needsPassword = nullptr);

private:
    static QString resolveArchive(const QString &archive, QString *error);
    static bool run(const QStringList &arguments, QString *stdOut, QString *error,
                    const QString &password, bool *needsPassword);
};
