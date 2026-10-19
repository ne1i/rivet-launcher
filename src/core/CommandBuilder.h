#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

class Game;
class Config;
class ProtonManager;

struct LaunchCommand {
    QString program;
    QStringList arguments;
    QProcessEnvironment environment;
    QString workingDirectory;
    QString error;
    QStringList warnings;
    QString protonName;

    bool isValid() const { return error.isEmpty() && !program.isEmpty(); }
    QString preview() const;
};

class CommandBuilder
{
public:
    CommandBuilder(Config *store, ProtonManager *protons);

    LaunchCommand build(const Game &game, const QString &debugValue = QString()) const;
    LaunchCommand stop(const Game &game) const;

private:
    Config *m_store;
    ProtonManager *m_protons;
};
