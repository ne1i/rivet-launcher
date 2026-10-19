#pragma once

#include "Config.h"

#include <QDialog>

class ProtonManager;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;

class GameSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    GameSettingsDialog(Config *store, ProtonManager *protons, const Game &game,
                       QWidget *parent = nullptr);

    QString gameName() const { return m_gameName; }

signals:
    void saved();

private:
    void buildUi();
    void reloadProtons();
    void updateEnvironmentLabel();
    void changeBannerFromFile();
    void changeBannerFromSteam();
    void changeIcon();
    void choosePrefix();
    void chooseExecutable();
    void chooseMainPath();
    void save();
    void updateBypassAvailability();

    Config *m_store;
    ProtonManager *m_protons;
    Game m_game;
    QString m_gameName;

    QLineEdit *m_name = nullptr;
    QLineEdit *m_executable = nullptr;
    QLineEdit *m_mainPath = nullptr;
    QLineEdit *m_prefixPath = nullptr;
    QLineEdit *m_steamId = nullptr;
    QLineEdit *m_fakeSteamId = nullptr;
    QLineEdit *m_argsBefore = nullptr;
    QLineEdit *m_argsAfter = nullptr;
    QLineEdit *m_overrides = nullptr;
    QComboBox *m_proton = nullptr;
    QCheckBox *m_overlay = nullptr;
    QCheckBox *m_runtime = nullptr;
    QCheckBox *m_wayland = nullptr;
    QCheckBox *m_wined3d = nullptr;
    QCheckBox *m_bypass = nullptr;
    QLabel *m_environmentLabel = nullptr;
    QLabel *m_bannerPreview = nullptr;
    QLabel *m_iconPreview = nullptr;
    QLabel *m_fixPathLabel = nullptr;
    QLabel *m_error = nullptr;

    QString m_bannerPath;
    QString m_iconPath;
    QString m_originalBanner;
    QString m_originalIcon;
    QMap<QString, QString> m_environment;
    bool m_originalBypass = false;
    QString m_originalPrefix;
};
