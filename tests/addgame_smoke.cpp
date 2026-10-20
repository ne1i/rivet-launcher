#include "core/Config.h"
#include "core/Installer.h"
#include "core/Paths.h"
#include "core/ProtonManager.h"
#include "core/Utils.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

bool writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(data) == data.size();
}

int fail(const QString &message)
{
    QTextStream(stderr) << "FAIL: " << message << "\n";
    return 1;
}

} // namespace

int main(int argc, char **argv)
{
    QTemporaryDir temporary;
    if (!temporary.isValid())
        return fail(QStringLiteral("could not create a temporary directory"));

    const QString home = temporary.path();
    qputenv("HOME", home.toUtf8());
    qputenv("XDG_CONFIG_HOME", (home + QStringLiteral("/.config")).toUtf8());
    qputenv("XDG_DATA_HOME", (home + QStringLiteral("/.local/share")).toUtf8());

    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("rivet-test"));

    // Create a legacy OFME config so migration is exercised.
    const QString legacyConfig = home + QStringLiteral("/.config/OFME-Linux");
    const QString legacyData = home
        + QStringLiteral("/.local/share/OnlineFix Linux Launcher");
    QDir().mkpath(legacyData + QStringLiteral("/protons"));
    const QString legacyGame = home + QStringLiteral("/LegacyGame");
    const QString legacyExecutable = legacyGame + QStringLiteral("/Legacy.exe");
    QDir().mkpath(legacyGame);
    if (!QFile::copy(QStringLiteral("/bin/true"), legacyExecutable))
        return fail(QStringLiteral("could not create the legacy executable"));

    const QByteArray legacyGames =
        "[Legacy Game]\n"
        "executable=" + legacyExecutable.toUtf8() + "\n"
        "mainPath=" + legacyGame.toUtf8() + "\n"
        "prefixPath=" + (legacyData + QStringLiteral("/prefixes/Legacy")).toUtf8() + "\n"
        "proton=GE-Proton Latest\n"
        "overrides=onlinefix64=n;steam_api64=n\n"
        "environment=LC_ALL====fr_FR.UTF-8\n"
        "steamID=1234\n"
        "fakeSteamID=480\n"
        "steamOverlay=1\n"
        "steamRuntime=1\n"
        "nativeWayland=1\n"
        "timeSpent=987\n";
    if (!writeFile(legacyConfig + QStringLiteral("/Games.ini"), legacyGames))
        return fail(QStringLiteral("could not write legacy Games.ini"));
    const QByteArray legacyLauncher =
        "[User Settings]\n"
        "defaultProton=GE-Proton Latest\n"
        "noSteamRequest=1\n"
        "fullscreen=\n"
        "downloadsSource=https://example.test/source.json\n";
    if (!writeFile(legacyConfig + QStringLiteral("/Launcher.ini"), legacyLauncher))
        return fail(QStringLiteral("could not write legacy Launcher.ini"));

    Config config;
    QString error;
    if (!config.load(&error))
        return fail(QStringLiteral("config load: %1").arg(error));
    if (!config.migrationNeeded())
        return fail(QStringLiteral("legacy migration was not offered"));
    if (!config.migrateLegacy(&error))
        return fail(QStringLiteral("legacy migration failed: %1").arg(error));
    if (!config.migratedFromLegacy())
        return fail(QStringLiteral("legacy config was not migrated"));
    if (config.migrationNeeded())
        return fail(QStringLiteral("migration prompt was not cleared"));

    const Game migrated = config.game(QStringLiteral("Legacy Game"));
    if (!migrated.isValid())
        return fail(QStringLiteral("migrated game is invalid"));
    if (migrated.timeSpent != 987)
        return fail(QStringLiteral("migrated play time is wrong"));
    if (migrated.environment.value(QStringLiteral("LC_ALL")) != QLatin1String("fr_FR.UTF-8"))
        return fail(QStringLiteral("legacy environment was not converted"));
    if (migrated.steamId != QLatin1String("1234") || migrated.fakeSteamId != QLatin1String("480"))
        return fail(QStringLiteral("Steam AppIDs were not migrated"));
    if (!migrated.nativeWayland || !migrated.steamRuntime || !migrated.steamOverlay)
        return fail(QStringLiteral("feature flags were not migrated"));
    if (migrated.overrides != QLatin1String("onlinefix64=n;steam_api64=n"))
        return fail(QStringLiteral("DLL overrides were not migrated"));
    if (config.downloadsSource() != QLatin1String("https://example.test/source.json"))
        return fail(QStringLiteral("download source was not migrated"));
    if (!config.noSteamRequest())
        return fail(QStringLiteral("launcher settings were not migrated"));
    if (config.protonsPath() != legacyData + QStringLiteral("/protons"))
        return fail(QStringLiteral("legacy Proton folder was not reused"));

    if (!QFileInfo::exists(Paths::configFile()))
        return fail(QStringLiteral("new config.json was not written"));
    if (!QFileInfo::exists(legacyConfig + QStringLiteral("/Games.ini")))
        return fail(QStringLiteral("legacy config was unexpectedly deleted"));

    // The new document must be structured JSON, not an INI file.
    QFile newConfig(Paths::configFile());
    if (!newConfig.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("could not read the new config"));
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(newConfig.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(QStringLiteral("new config is not valid JSON"));
    if (document.object().value(QStringLiteral("format")).toString() != QLatin1String("rivet"))
        return fail(QStringLiteral("new config has the wrong format marker"));
    if (document.object().value(QStringLiteral("games")).toArray().size() != 1)
        return fail(QStringLiteral("new config has the wrong game count"));

    // The add-game pipeline still works on top of the new config.
    const QString gameDir = home + QStringLiteral("/Game");
    const QString executable = gameDir + QStringLiteral("/Game.exe");
    QDir().mkpath(gameDir);
    if (!QFile::copy(QStringLiteral("/bin/true"), executable))
        return fail(QStringLiteral("could not create the fake executable"));
    QFile::setPermissions(executable, QFile::permissions(executable) | QFileDevice::ExeOwner);
    if (!writeFile(gameDir + QStringLiteral("/OnlineFix64.dll"), "not-a-real-dll"))
        return fail(QStringLiteral("could not write the fake fix DLL"));
    if (!writeFile(gameDir + QStringLiteral("/OnlineFix.ini"),
                   "[Main]\nRealAppId=480\nFakeAppId=480\n\n[Misc]\nExtraProtection=true\n"))
        return fail(QStringLiteral("could not write the fake ini"));

    ProtonManager protons(&config);
    Installer installer(&config, &protons);

    AddGameRequest request;
    request.sourceRoot = gameDir;
    request.candidate = executable;
    request.name = QStringLiteral("Test Game");
    request.installPath = gameDir;
    request.prefixPath = home + QStringLiteral("/prefixes/Test Game");
    request.cleanAfterAdd = false;

    AddGameResult result = installer.addGame(request, [](const QString &) {});
    if (!result.success)
        return fail(QStringLiteral("installer: %1").arg(result.error));
    if (!result.game.overrides.contains(QStringLiteral("onlinefix64=n")))
        return fail(QStringLiteral("missing onlinefix override: %1").arg(result.game.overrides));
    if (result.game.fakeSteamId != QLatin1String("480"))
        return fail(QStringLiteral("wrong fake app id: %1").arg(result.game.fakeSteamId));

    result.game.environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("fr_FR.UTF-8"));
    if (!config.saveGame(result.game, &error))
        return fail(QStringLiteral("save game: %1").arg(error));

    Config reloaded;
    if (!reloaded.load(&error))
        return fail(QStringLiteral("reload: %1").arg(error));
    const Game saved = reloaded.game(QStringLiteral("Test Game"));
    if (saved.executable != executable)
        return fail(QStringLiteral("the game did not survive the JSON round-trip"));
    if (saved.environment.value(QStringLiteral("LC_ALL")) != QLatin1String("fr_FR.UTF-8"))
        return fail(QStringLiteral("game environment was lost"));

    QTextStream(stdout) << "PASS: config migration and add-game pipeline\n";
    return 0;
}
