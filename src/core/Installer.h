#pragma once

#include "Config.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

class ProtonManager;

struct CandidateFile {
    QString absolutePath;
    QString relativePath;
    QString archiveMember;
    bool archive = false;
    bool installable = false;
};

struct AddGameRequest {
    QString sourceRoot;     // directory the user picked
    QString candidate;      // selected executable or archive
    QString archiveMember;  // optional path inside an archive
    QString archivePassword;
    QString name;
    QString installPath;    // destination for archives / installers
    QString prefixPath;
    QString installerPath;  // optional FreeTP installer
    bool canInstall = false; // run candidate + installer through Proton
    bool cleanAfterAdd = false;
};

struct AddGameResult {
    bool success = false;
    QString error;
    Game game;
    QStringList messages;
};

class AddGameScanner
{
public:
    static QVector<CandidateFile> scan(const QString &root);
    static bool isJunk(const QString &fileName);
    static bool isAutoSelectable(const QString &absolutePath);
    static QString findMainExecutable(const QString &directory, const QString &hint = QString());
};

class Installer
{
public:
    Installer(Config *store, ProtonManager *protons);

    using ProgressCallback = std::function<void(const QString &message)>;

    AddGameResult addGame(const AddGameRequest &request, ProgressCallback progress) const;
    AddGameResult installFreeTp(const AddGameRequest &request, ProgressCallback progress) const;

private:
    bool runInstaller(const QString &installer, const QString &installPath,
                      const QString &prefixPath, ProgressCallback progress, QString *error) const;
    AddGameResult finishGame(const AddGameRequest &request, const QString &root,
                             const QString &executable, ProgressCallback progress) const;

    Config *m_store;
    ProtonManager *m_protons;
};
