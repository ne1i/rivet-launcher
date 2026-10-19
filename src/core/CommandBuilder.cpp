#include "CommandBuilder.h"

#include "Config.h"
#include "ProtonManager.h"
#include "Steam.h"
#include "Utils.h"

#include <QDir>
#include <QFileInfo>

namespace {

const QString kDisabledD3D =
    QStringLiteral("d3d11=n;d3d10=n;d3d10core=n;dxgi=n;openvr_api_dxvk=n;"
                   "d3d12=n;d3d12core=n;d3d9=n;d3d8=n;");

} // namespace

CommandBuilder::CommandBuilder(Config *store, ProtonManager *protons)
    : m_store(store)
    , m_protons(protons)
{
}

LaunchCommand CommandBuilder::build(const Game &game, const QString &debugValue) const
{
    LaunchCommand command;

    if (!QFileInfo(game.executable).isFile()) {
        command.error = QStringLiteral("The game executable was not found:\n%1\n\n"
                                       "Maybe it was moved or deleted.")
                            .arg(game.executable);
        return command;
    }

    if (!m_protons) {
        command.error = QStringLiteral("Proton is not configured");
        return command;
    }

    const QString protonName = m_protons->resolve(game.proton);
    if (protonName.isEmpty()) {
        command.error = QStringLiteral("Could not find Proton for this game. Open the launcher "
                                       "settings → Proton and install a version.");
        return command;
    }
    command.protonName = protonName;

    const QString proton = m_protons->executable(protonName, QStringLiteral("proton"));
    if (proton.isEmpty()) {
        command.error = QStringLiteral("The Proton executable is missing from %1")
                            .arg(m_protons->pathFor(protonName));
        return command;
    }

    const QString prefixRoot = game.effectivePrefixPath();
    if (!QDir().mkpath(prefixRoot + QStringLiteral("/pfx"))) {
        command.error = QStringLiteral("Could not create the prefix directory:\n%1").arg(prefixRoot);
        return command;
    }

    QStringList environmentArguments;
    environmentArguments << QStringLiteral("run") << game.executable;
    environmentArguments << Utils::splitArguments(game.argsAfter);

    const QString runtime = game.steamRuntime ? Steam::findRuntime(m_protons->pathFor(protonName))
                                              : QString();
    if (game.steamRuntime && runtime.isEmpty())
        command.warnings.append(QStringLiteral("Steam Runtime is not installed for this Proton build; "
                                               "launching without it."));
    QStringList fullCommand;
    if (!runtime.isEmpty()) {
        fullCommand << runtime << QStringLiteral("--") << proton << environmentArguments;
    } else {
        fullCommand << proton << environmentArguments;
    }

    const QStringList before = Utils::splitArguments(game.argsBefore);
    if (!before.isEmpty())
        fullCommand = before + fullCommand;

    command.program = fullCommand.takeFirst();
    command.arguments = fullCommand;

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    QString overrides = game.overrides.trimmed();
    if (!game.wined3d) {
        if (!overrides.isEmpty())
            overrides = kDisabledD3D + overrides;
        else
            overrides = kDisabledD3D;
    }
    if (!overrides.isEmpty())
        environment.insert(QStringLiteral("WINEDLLOVERRIDES"), overrides);
    else
        environment.remove(QStringLiteral("WINEDLLOVERRIDES"));

    environment.insert(QStringLiteral("WINEDEBUG"),
                       debugValue.isEmpty() ? QStringLiteral("-all") : debugValue);
    environment.insert(QStringLiteral("STEAM_COMPAT_DATA_PATH"), prefixRoot);
    environment.insert(QStringLiteral("STEAM_COMPAT_CLIENT_INSTALL_PATH"), Steam::installPath());
    if (game.wined3d)
        environment.insert(QStringLiteral("PROTON_USE_WINED3D"), QStringLiteral("1"));
    else
        environment.remove(QStringLiteral("PROTON_USE_WINED3D"));
    if (game.nativeWayland)
        environment.insert(QStringLiteral("PROTON_ENABLE_WAYLAND"), QStringLiteral("1"));
    else
        environment.remove(QStringLiteral("PROTON_ENABLE_WAYLAND"));

    if (game.steamOverlay) {
        const QString renderer32 = Steam::overlayRenderer32();
        const QString renderer64 = Steam::overlayRenderer64();
        environment.insert(QStringLiteral("LD_PRELOAD"),
                           QStringLiteral(":%1:%2").arg(renderer32, renderer64));
        environment.insert(QStringLiteral("ENABLE_VK_LAYER_VALVE_steam_overlay_1"),
                           QStringLiteral("1"));
        environment.insert(QStringLiteral("SteamOverlayGameId"),
                           game.fakeSteamId.isEmpty() ? QStringLiteral("480") : game.fakeSteamId);
        if (!QFileInfo(renderer64).isFile())
            command.warnings.append(QStringLiteral("Steam overlay libraries were not found. "
                                                   "Check that Steam is installed as a native package."));
    }

    for (auto it = game.environment.constBegin(); it != game.environment.constEnd(); ++it)
        environment.insert(it.key(), it.value());

    command.environment = environment;
    command.workingDirectory = game.workingDirectory();
    return command;
}

LaunchCommand CommandBuilder::stop(const Game &game) const
{
    LaunchCommand command;
    if (!m_protons) {
        command.error = QStringLiteral("Proton is not configured");
        return command;
    }

    const QString protonName = m_protons->resolve(game.proton);
    const QString wineserver = protonName.isEmpty()
        ? QString()
        : m_protons->executable(protonName, QStringLiteral("wineserver"));

    if (!wineserver.isEmpty()) {
        command.program = wineserver;
        command.arguments = {QStringLiteral("-k")};
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("WINEPREFIX"), game.winePrefixPath());
        command.environment = environment;
        return command;
    }

    // Fallback used by the original launcher when the prefix has not been
    // created yet.
    command.program = QStringLiteral("pkill");
    command.arguments = {QStringLiteral("-f"), QFileInfo(game.executable).fileName()};
    command.environment = QProcessEnvironment::systemEnvironment();
    return command;
}
