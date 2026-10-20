#include "core/CommandBuilder.h"
#include "core/Config.h"
#include "core/ProtonManager.h"
#include "core/Runner.h"
#include "core/Utils.h"
#include "ui/Icons.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QTextStream>

#include <unistd.h>

namespace {

QString socketName()
{
    return QStringLiteral("rivet-%1").arg(getuid());
}

void printHelp()
{
    QTextStream out(stdout);
    out << "Rivet " << QCoreApplication::applicationVersion() << "\n\n"
        << "Usage:\n"
        << "  rivet                              Open the launcher\n"
        << "  rivet --launch \"Game name\"          Launch a game directly\n"
        << "  rivet \"Game name\"                   Same as --launch\n"
        << "  rivet --list                       List games in the library\n"
        << "  rivet --print-command \"Game name\"   Show the Proton command and environment\n"
        << "  rivet --version                    Show the version\n\n"
        << "Options:\n"
        << "  --debug VALUE                     Use VALUE as WINEDEBUG for --print-command\n";
}

struct Options {
    bool help = false;
    bool version = false;
    bool list = false;
    bool printCommand = false;
    QString launch;
    QString debug;
};

Options parseOptions(const QStringList &arguments)
{
    Options options;
    for (int i = 1; i < arguments.size(); ++i) {
        const QString arg = arguments[i];
        if (arg == QLatin1String("--help") || arg == QLatin1String("-h")) {
            options.help = true;
        } else if (arg == QLatin1String("--version")) {
            options.version = true;
        } else if (arg == QLatin1String("--list")) {
            options.list = true;
        } else if (arg == QLatin1String("--print-command")) {
            options.printCommand = true;
            if (i + 1 < arguments.size()) {
                options.launch = arguments[++i];
            }
        } else if (arg == QLatin1String("--launch")) {
            if (i + 1 < arguments.size())
                options.launch = arguments[++i];
        } else if (arg == QLatin1String("--debug")) {
            if (i + 1 < arguments.size())
                options.debug = arguments[++i];
        } else if (!arg.startsWith(QLatin1Char('-'))) {
            if (options.launch.isEmpty())
                options.launch = arg;
        }
    }
    return options;
}

int runConsoleCommand(const Options &options)
{
    Config store;
    QString error;
    if (!store.load(&error)) {
        QTextStream(stderr) << "Could not load the library: " << error << "\n";
        return 1;
    }

    QTextStream out(stdout);

    if (options.list) {
        const QStringList names = store.gameNames();
        for (const QString &name : names)
            out << name << '\t' << store.game(name).executable << '\n';
        return 0;
    }

    if (options.printCommand) {
        ProtonManager protons(&store);
        CommandBuilder builder(&store, &protons);
        const Game game = store.game(options.launch);
        if (!game.isValid()) {
            QTextStream(stderr) << "Game not found: " << options.launch << "\n";
            return 2;
        }
        const LaunchCommand command = builder.build(game, options.debug);
        if (!command.isValid()) {
            QTextStream(stderr) << command.error << "\n";
            return 2;
        }
        out << "Program: " << command.program << '\n';
        out << "Working directory: " << command.workingDirectory << '\n';
        out << "Arguments:\n";
        for (const QString &argument : command.arguments)
            out << "  " << argument << '\n';
        out << "Environment:\n";
        const QStringList keys = command.environment.keys();
        for (const QString &key : keys)
            out << "  " << key << '=' << command.environment.value(key) << '\n';
        if (!command.warnings.isEmpty()) {
            out << "Warnings:\n";
            for (const QString &warning : command.warnings)
                out << "  " << warning << '\n';
        }
        return 0;
    }

    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    const QStringList arguments = [&] {
        QStringList list;
        for (int i = 0; i < argc; ++i)
            list.append(QString::fromLocal8Bit(argv[i]));
        return list;
    }();

    const Options options = parseOptions(arguments);

    const bool consoleMode = options.help || options.version || options.list
        || options.printCommand;

    if (consoleMode) {
        QCoreApplication app(argc, argv);
        QCoreApplication::setApplicationName(QStringLiteral("Rivet"));
        QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
        QCoreApplication::setOrganizationName(QStringLiteral("Rivet"));
        if (options.help) {
            printHelp();
            return 0;
        }
        if (options.version) {
            QTextStream(stdout) << QCoreApplication::applicationVersion() << '\n';
            return 0;
        }
        return runConsoleCommand(options);
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Rivet"));
    QApplication::setApplicationDisplayName(QStringLiteral("Rivet"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QApplication::setOrganizationName(QStringLiteral("Rivet"));
    QApplication::setDesktopFileName(QStringLiteral("rivet"));
    QApplication::setWindowIcon(Icons::appIcon());
    QApplication::setQuitOnLastWindowClosed(false);

    Theme::apply(app);

    // Hand a launch/show request to an instance that is already running. This
    // is what makes desktop shortcuts work while the launcher is in the tray.
    const bool hasLaunch = !options.launch.isEmpty();
    {
        QLocalSocket socket;
        socket.connectToServer(socketName(), QIODevice::WriteOnly);
        if (socket.waitForConnected(400)) {
            const QString message = hasLaunch
                ? QStringLiteral("LAUNCH\t%1\n").arg(options.launch)
                : QStringLiteral("SHOW\n");
            socket.write(message.toUtf8());
            socket.flush();
            socket.waitForBytesWritten(600);
            return 0;
        }
    }

    Config store;
    QString loadError;
    if (!store.load(&loadError)) {
        QMessageBox::critical(nullptr, QStringLiteral("Rivet"), loadError);
        return 1;
    }

    if (store.migrationNeeded()) {
        QMessageBox box(QMessageBox::Question,
                        QStringLiteral("Migrate existing configuration?"),
                        QStringLiteral(
                            "An OnlineFix launcher configuration was found.\n\n"
                            "Import its games, play time, covers, icons and settings into Rivet?\n\n"
                            "The old files will not be deleted."),
                        QMessageBox::Yes | QMessageBox::No, nullptr);
        auto *never = new QCheckBox(QStringLiteral("Never show this again"));
        box.setCheckBox(never);
        box.setDefaultButton(QMessageBox::Yes);
        const int answer = box.exec();
        if (answer == QMessageBox::Yes) {
            QString migrationError;
            if (!store.migrateLegacy(&migrationError)) {
                QMessageBox::critical(nullptr, QStringLiteral("Migration failed"), migrationError);
            }
        } else {
            store.declineMigrationPrompt(never->isChecked());
        }
    }

    ProtonManager protons(&store);
    Runner runner(&store, &protons);
    MainWindow window(&store, &protons, &runner, hasLaunch, options.launch);

    QLocalServer server;
    QLocalServer::removeServer(socketName());
    if (!server.listen(socketName()))
        qWarning("Could not start the single-instance server: %s",
                 qPrintable(server.errorString()));
    QObject::connect(&server, &QLocalServer::newConnection, &window, [&server, &window] {
        while (QLocalSocket *socket = server.nextPendingConnection()) {
            QObject::connect(socket, &QLocalSocket::readyRead, &window, [socket, &window] {
                const QString message = QString::fromUtf8(socket->readAll()).trimmed();
                if (!message.isEmpty())
                    window.handleIpcCommand(message);
                socket->disconnectFromServer();
                socket->deleteLater();
            });
            QObject::connect(socket, &QLocalSocket::disconnected, socket,
                             &QLocalSocket::deleteLater);
        }
    });

    if (!hasLaunch) {
        if (store.fullscreen())
            window.showFullScreen();
        else
            window.show();
    }


    return app.exec();
}
