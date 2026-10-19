#pragma once

#include <QString>
#include <QStringList>

struct Game;

struct FixInfo {
    QString overrides;
    QString realAppId;
    QString fakeAppId;
    QString fixPath;
    bool freeTpPatched = false;
    bool photonPatched = false;
    QStringList warnings;
};

class FixParser
{
public:
    // Scans a game directory for OnlineFix / FreeTP / EOSFix files, applies the
    // same compatibility patches as the original launcher, and returns the
    // resulting DLL overrides and AppIDs.
    static FixInfo parse(const QString &root, QString *error = nullptr);

    static QString fetchBanner(const QString &appId, QString *error = nullptr);

    static bool isNoOfllPathEnabled(const QString &fixPath);
    static bool setNoOfllPath(const QString &fixPath, bool enable, QString *error = nullptr);
};
