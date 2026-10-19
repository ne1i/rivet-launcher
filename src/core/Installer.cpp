#include "Installer.h"

#include "FixParser.h"
#include "IconExtractor.h"
#include "Paths.h"
#include "ProtonManager.h"
#include "RarExtractor.h"
#include "Steam.h"
#include "Utils.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace {

const QStringList &junkPatterns()
{
    static const QStringList patterns = {
        // Visual C++ redistributables
        QStringLiteral("^vcredist.*\\.exe$"),
        QStringLiteral("^vc_redist.*\\.exe$"),
        // DirectX
        QStringLiteral("^dxsetup\\.exe$"),
        QStringLiteral("^directx.*setup.*\\.exe$"),
        QStringLiteral("^dxwebsetup\\.exe$"),
        // .NET
        QStringLiteral("^dotnet.*setup.*\\.exe$"),
        QStringLiteral("^ndp[A-Za-z0-9._-]*(?=.*-KB\\d+)(?=.*-(?:x86|x64))(?=.*-AllOS)(?=.*-ENU)[A-Za-z0-9._-]*\\.exe$"),
        // Installers / uninstallers
        QStringLiteral("^install.*\\.exe$"),
        QStringLiteral("^uninstall.*\\.exe$"),
        QStringLiteral("^unins[0-9]+\\.exe$"),
        // Updaters / patchers
        QStringLiteral("^updater.*\\.exe$"),
        QStringLiteral("^patch.*\\.exe$"),
        // Store clients
        QStringLiteral("^steam\\.exe$"),
        QStringLiteral("^origin.*\\.exe$"),
        QStringLiteral("^uplay.*\\.exe$"),
        QStringLiteral("^epicgames.*\\.exe$"),
        QStringLiteral("^gog.*\\.exe$"),
        // Unity
        QStringLiteral("^unitycrashhandler.*\\.exe$"),
        QStringLiteral("^unitybugreporter.*\\.exe$"),
        QStringLiteral("^winpixeventruntime.*\\.exe$"),
        QStringLiteral("^monodistribution.*\\.exe$"),
        // Unreal
        QStringLiteral("^ue4.*prereq.*\\.exe$"),
        QStringLiteral("^crashreportclient.*\\.exe$"),
        QStringLiteral("^bugreporter.*\\.exe$"),
        // CryEngine / editors
        QStringLiteral("^sandbox.*\\.exe$"),
        QStringLiteral("^editor.*\\.exe$"),
        // Tools
        QStringLiteral("^config.*\\.exe$"),
        QStringLiteral("^settings.*\\.exe$"),
        QStringLiteral("^options.*\\.exe$"),
        QStringLiteral("^benchmark.*\\.exe$"),
        QStringLiteral("^profil(er|ing).*\\.exe$"),
        QStringLiteral("^crashpad_handler\\.exe$"),
    };
    return patterns;
}


QString baseArchive(const QString &fileName)
{
    static const QRegularExpression partRegex(QStringLiteral("\\.part\\d+\\.rar$"),
                                               QRegularExpression::CaseInsensitiveOption);
    QString base = fileName;
    base.remove(partRegex);
    return base + QStringLiteral(".rar");
}

QString windowsPath(const QString &path)
{
    QString result = path;
    result.replace(QLatin1Char('/'), QLatin1Char('\\'));
    return QStringLiteral("Z:") + result;
}

QStringList installersFor(const AddGameRequest &request)
{
    QStringList installers;
    if (request.canInstall && !request.candidate.isEmpty())
        installers.append(request.candidate);
    if (!request.installerPath.isEmpty() && !installers.contains(request.installerPath))
        installers.append(request.installerPath);
    return installers;
}

} // namespace

bool AddGameScanner::isJunk(const QString &fileName)
{
    for (const QString &pattern : junkPatterns()) {
        if (QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption)
                .match(fileName)
                .hasMatch())
            return true;
    }
    return false;
}

bool AddGameScanner::isAutoSelectable(const QString &absolutePath)
{
    const QFileInfo info(absolutePath);
    const QString lower = info.fileName().toLower();
    if (lower == QLatin1String("eosauthlauncher.exe"))
        return true;
    if (lower == QLatin1String("launcher.exe"))
        return true;
    return false;
}

QVector<CandidateFile> AddGameScanner::scan(const QString &root)
{
    QVector<CandidateFile> candidates;
    if (!QFileInfo(root).isDir())
        return candidates;

    QSet<QString> seen;
    QDirIterator iterator(root, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext() && candidates.size() < 5000) {
        const QString path = iterator.next();
        if (path.contains(QStringLiteral("/OFME Prefix/")))
            continue;

        const QFileInfo info(path);
        const QString suffix = info.suffix().toLower();
        if (suffix != QLatin1String("exe") && suffix != QLatin1String("vbs")
            && suffix != QLatin1String("bat") && suffix != QLatin1String("rar"))
            continue;

        if (isJunk(info.fileName()))
            continue;

        if (suffix == QLatin1String("rar")) {
            if (info.fileName().contains(QLatin1String(".part"), Qt::CaseInsensitive)) {
                if (QFileInfo(info.absolutePath() + QLatin1Char('/') + baseArchive(info.fileName())).isFile())
                    continue;
            }
            if (info.absolutePath().endsWith(QLatin1String("/Fix Repair")))
                continue;
        }

        const QString absolute = info.absoluteFilePath();
        if (seen.contains(absolute))
            continue;
        seen.insert(absolute);

        CandidateFile candidate;
        candidate.absolutePath = absolute;
        candidate.relativePath = QDir(root).relativeFilePath(absolute);
        candidate.archive = suffix == QLatin1String("rar");
        if (candidate.archive) {
            candidate.installable = true;
        } else if (suffix == QLatin1String("exe")) {
            const QDir dir(info.absolutePath());
            candidate.installable = !dir.entryList({QStringLiteral("*.ftp")}, QDir::Files).isEmpty();
        }
        candidates.append(candidate);
    }

    std::sort(candidates.begin(), candidates.end(), [](const CandidateFile &a, const CandidateFile &b) {
        if (a.archive != b.archive)
            return !a.archive;
        return a.relativePath.compare(b.relativePath, Qt::CaseInsensitive) < 0;
    });
    return candidates;
}

QString AddGameScanner::findMainExecutable(const QString &directory, const QString &hint)
{
    if (!hint.isEmpty()) {
        const QString direct = directory + QLatin1Char('/') + hint;
        if (QFileInfo(direct).isFile())
            return QFileInfo(direct).absoluteFilePath();
    }

    const QVector<CandidateFile> candidates = scan(directory);
    QVector<CandidateFile> executables;
    for (const CandidateFile &candidate : candidates) {
        if (!candidate.archive)
            executables.append(candidate);
    }
    if (executables.isEmpty())
        return {};

    for (const CandidateFile &candidate : executables) {
        if (isAutoSelectable(candidate.absolutePath))
            return candidate.absolutePath;
    }

    // Prefer a top-level executable; deep binaries are often tools.
    std::sort(executables.begin(), executables.end(),
              [](const CandidateFile &a, const CandidateFile &b) {
                  const int depthA = a.relativePath.count(QLatin1Char('/'));
                  const int depthB = b.relativePath.count(QLatin1Char('/'));
                  if (depthA != depthB)
                      return depthA < depthB;
                  return a.relativePath.size() < b.relativePath.size();
              });
    return executables.first().absolutePath;
}

Installer::Installer(Config *store, ProtonManager *protons)
    : m_store(store)
    , m_protons(protons)
{
}

AddGameResult Installer::addGame(const AddGameRequest &request, ProgressCallback progress) const
{
    AddGameResult result;

    if (request.name.trimmed().isEmpty()) {
        result.error = QStringLiteral("The game name is empty.");
        return result;
    }
    if (m_store->hasGame(request.name)) {
        result.error = QStringLiteral("A game named \"%1\" already exists.").arg(request.name);
        return result;
    }

    const QFileInfo candidateInfo(request.candidate);
    QString root;
    QString executable;

    if (request.canInstall && !installersFor(request).isEmpty()) {
        if (progress)
            progress(QStringLiteral("Running the installer through Proton…"));
        QString error;
        const QStringList installers = installersFor(request);
        for (const QString &installer : installers) {
            if (!runInstaller(installer, request.installPath, request.prefixPath, progress, &error)) {
                result.error = error;
                return result;
            }
        }
        root = request.installPath;
        if (!QFileInfo(root).isDir() || QDir(root).isEmpty()) {
            result.error = QStringLiteral("The installer finished, but the install folder is empty:\n%1")
                               .arg(root);
            return result;
        }
        executable = AddGameScanner::findMainExecutable(root);
    } else if (candidateInfo.suffix().compare(QLatin1String("rar"), Qt::CaseInsensitive) == 0) {
        if (progress)
            progress(QStringLiteral("Unpacking %1…").arg(candidateInfo.fileName()));
        QString error;
        bool needsPassword = false;
        if (!RarExtractor::extract(request.candidate, request.installPath, &error,
                                  request.archivePassword, &needsPassword)) {
            result.error = needsPassword
                ? QStringLiteral("The archive needs a password, or the password is wrong.")
                : error;
            return result;
        }
        root = request.installPath;
        executable = AddGameScanner::findMainExecutable(root, request.archiveMember);
    } else {
        root = request.sourceRoot;
        executable = request.candidate;
    }

    if (executable.isEmpty() || !QFileInfo(executable).isFile()) {
        result.error = QStringLiteral("Could not find a game executable in:\n%1").arg(root);
        return result;
    }

    result = finishGame(request, root, QFileInfo(executable).absoluteFilePath(), progress);
    if (!result.success)
        return result;

    if (request.cleanAfterAdd) {
        const QFileInfo sourceFile(request.candidate);
        QString sourceDirectory = sourceFile.absolutePath();
        if (sourceFile.isFile() && !request.canInstall
            && sourceFile.suffix().compare(QLatin1String("rar"), Qt::CaseInsensitive) != 0)
            sourceDirectory.clear(); // plain add: never delete the game files

        const QString install = QFileInfo(request.installPath).absoluteFilePath();
        if (!sourceDirectory.isEmpty() && sourceDirectory != QDir::homePath()
            && !Utils::isPathInside(install, sourceDirectory)
            && !Utils::isPathInside(sourceDirectory, install)) {
            const QString downloads = QDir(m_store->downloadsPath()).absolutePath();
            const bool sharedFolder = sourceDirectory == downloads
                || sourceDirectory == QDir::tempPath() || sourceDirectory == QDir::homePath();
            if (sharedFolder) {
                // Never delete a whole shared folder such as ~/Downloads. Remove
                // only the archive/setup files the user selected.
                QFile::remove(request.candidate);
                if (!request.installerPath.isEmpty()
                    && QFileInfo(request.installerPath).absolutePath() == sourceDirectory)
                    QFile::remove(request.installerPath);
            } else {
                QString error;
                if (!Utils::removeRecursively(sourceDirectory, &error))
                    result.messages.append(QStringLiteral("Could not remove install files: %1").arg(error));
            }
        } else if (!sourceDirectory.isEmpty()) {
            result.messages.append(QStringLiteral("Install files were kept because they overlap the game folder."));
        }
    }

    return result;
}

AddGameResult Installer::installFreeTp(const AddGameRequest &request, ProgressCallback progress) const
{
    return addGame(request, progress);
}

bool Installer::runInstaller(const QString &installer, const QString &installPath,
                             const QString &prefixPath, ProgressCallback progress,
                             QString *error) const
{
    if (!QFileInfo(installer).isFile()) {
        if (error)
            *error = QStringLiteral("Installer not found: %1").arg(installer);
        return false;
    }

    const QString protonName = m_protons->resolve(m_store->defaultProton());
    const QString proton = protonName.isEmpty()
        ? QString()
        : m_protons->executable(protonName, QStringLiteral("proton"));
    if (proton.isEmpty()) {
        if (error)
            *error = QStringLiteral("Proton is not installed. Install a Proton build in the launcher settings first.");
        return false;
    }

    QDir().mkpath(prefixPath);
    QDir().mkpath(installPath);

    if (progress)
        progress(QStringLiteral("Installing with %1…").arg(QFileInfo(installer).fileName()));

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("STEAM_COMPAT_DATA_PATH"), prefixPath);
    environment.insert(QStringLiteral("STEAM_COMPAT_CLIENT_INSTALL_PATH"), Steam::installPath());
    environment.insert(QStringLiteral("WINEDEBUG"), QStringLiteral("-all"));

    const Utils::ProcessResult result = Utils::runProcess(
        proton,
        {QStringLiteral("run"), installer,
         QStringLiteral("/DIR=") + windowsPath(installPath),
         QStringLiteral("/TASKS="), QStringLiteral("/SILENT")},
        QFileInfo(installer).absolutePath(), environment, 60 * 60 * 1000);

    if (!result.ok()) {
        if (error) {
            *error = QStringLiteral("The installer failed (exit code %1).\n\n%2")
                         .arg(result.exitCode)
                         .arg(result.stdErr.trimmed());
        }
        return false;
    }
    return true;
}

AddGameResult Installer::finishGame(const AddGameRequest &request, const QString &root,
                                    const QString &executable, ProgressCallback progress) const
{
    AddGameResult result;

    if (progress)
        progress(QStringLiteral("Looking for the multiplayer fix…"));
    QString parseError;
    const FixInfo fix = FixParser::parse(root, &parseError);
    result.messages.append(fix.warnings);

    QString banner;
    if (!fix.realAppId.isEmpty()) {
        if (progress)
            progress(QStringLiteral("Downloading the Steam cover…"));
        QString bannerError;
        banner = FixParser::fetchBanner(fix.realAppId, &bannerError);
        if (banner.isEmpty() && !bannerError.isEmpty())
            result.messages.append(QStringLiteral("Cover: %1").arg(bannerError));
    }

    QString icon;
    if (progress)
        progress(QStringLiteral("Extracting the game icon…"));
    QString iconError;
    icon = IconExtractor::extractFromExecutable(executable, &iconError);
    if (icon.isEmpty() && !iconError.isEmpty())
        result.messages.append(QStringLiteral("Icon: %1").arg(iconError));

    Game game;
    game.name = request.name;
    game.executable = executable;
    game.mainPath = root;
    game.prefixPath = request.prefixPath;
    game.proton = m_store->defaultProton();
    game.banner = banner;
    game.icon = icon;
    game.overrides = fix.overrides;
    game.fixPath = fix.fixPath;
    game.fakeSteamId = fix.fakeAppId;
    game.steamId = fix.realAppId;
    game.steamOverlay = true;
    const QString protonName = m_protons->resolve(game.proton);
    game.steamRuntime = !protonName.isEmpty()
        && !Steam::findRuntime(m_protons->pathFor(protonName)).isEmpty();
    game.wined3d = m_store->gamesUseWined3d();
    game.nativeWayland = m_store->gamesUseWayland();

    result.game = game;
    result.success = true;
    return result;
}
