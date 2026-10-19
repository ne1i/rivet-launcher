#include "GameSettingsDialog.h"

#include "DesktopEntry.h"
#include "EnvDialog.h"
#include "FixParser.h"
#include "Icons.h"
#include "IconExtractor.h"
#include "LauncherSettingsDialog.h"
#include "Paths.h"
#include "ProtonManager.h"
#include "Steam.h"
#include "Theme.h"
#include "Utils.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

QString uniqueBannerPath(const QString &gameName, const QString &extension)
{
    Paths::ensureDir(Paths::coversDir());
    const QString base = gameName.isEmpty() ? QStringLiteral("cover") : gameName;
    QString candidate = Paths::coversDir() + QLatin1Char('/') + base + QLatin1Char('.') + extension;
    int counter = 1;
    while (QFileInfo::exists(candidate))
        candidate = Paths::coversDir() + QLatin1Char('/') + base + QLatin1Char('-')
            + QString::number(counter++) + QLatin1Char('.') + extension;
    return candidate;
}

QLabel *hintLabel(const QString &text)
{
    auto *label = new QLabel(text);
    label->setObjectName(QStringLiteral("small"));
    label->setWordWrap(true);
    return label;
}

QWidget *lineEditWithButton(QLineEdit *edit, QPushButton *button)
{
    auto *container = new QWidget;
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(edit, 1);
    layout->addWidget(button);
    return container;
}

} // namespace

GameSettingsDialog::GameSettingsDialog(Config *store, ProtonManager *protons, const Game &game,
                                       QWidget *parent)
    : QDialog(parent)
    , m_store(store)
    , m_protons(protons)
    , m_game(game)
    , m_gameName(game.name)
    , m_bannerPath(game.banner)
    , m_iconPath(game.icon)
    , m_originalBanner(game.banner)
    , m_originalIcon(game.icon)
    , m_environment(game.environment)
    , m_originalPrefix(game.prefixPath)
{
    setWindowTitle(QStringLiteral("Game settings — %1").arg(game.name));
    setMinimumSize(860, 680);
    resize(900, 720);
    m_originalBypass = FixParser::isNoOfllPathEnabled(game.fixPath);
    buildUi();
}

void GameSettingsDialog::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(26, 22, 26, 20);
    layout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Settings for %1").arg(m_game.name));
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);

    auto *subtitle = new QLabel(QStringLiteral(
        "Everything here is saved to the game's entry. Changes apply the next time you launch it."));
    subtitle->setObjectName(QStringLiteral("muted"));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    auto *tabs = new QTabWidget;

    // ---- General ------------------------------------------------------
    auto *generalScroll = new QScrollArea;
    generalScroll->setWidgetResizable(true);
    auto *general = new QWidget;
    auto *generalLayout = new QVBoxLayout(general);
    generalLayout->setContentsMargins(2, 12, 12, 12);
    generalLayout->setSpacing(16);

    auto *identity = new QGroupBox(QStringLiteral("Game"));
    auto *identityForm = new QFormLayout(identity);
    identityForm->setSpacing(10);

    m_name = new QLineEdit(m_game.name);
    identityForm->addRow(QStringLiteral("Name"), m_name);

    m_executable = new QLineEdit(m_game.executable);
    auto *execBrowse = new QPushButton(QStringLiteral("Browse…"));
    connect(execBrowse, &QPushButton::clicked, this, &GameSettingsDialog::chooseExecutable);
    identityForm->addRow(QStringLiteral("Executable"), lineEditWithButton(m_executable, execBrowse));

    m_mainPath = new QLineEdit(m_game.mainPath);
    auto *mainBrowse = new QPushButton(QStringLiteral("Browse…"));
    connect(mainBrowse, &QPushButton::clicked, this, &GameSettingsDialog::chooseMainPath);
    identityForm->addRow(QStringLiteral("Game folder"), lineEditWithButton(m_mainPath, mainBrowse));

    m_prefixPath = new QLineEdit(m_game.effectivePrefixPath());
    auto *prefixBrowse = new QPushButton(QStringLiteral("Browse…"));
    connect(prefixBrowse, &QPushButton::clicked, this, &GameSettingsDialog::choosePrefix);
    identityForm->addRow(QStringLiteral("Prefix folder"),
                         lineEditWithButton(m_prefixPath, prefixBrowse));

    m_steamId = new QLineEdit(m_game.steamId);
    m_steamId->setPlaceholderText(QStringLiteral("Optional, used for links and covers"));
    identityForm->addRow(QStringLiteral("Steam AppID"), m_steamId);

    m_fakeSteamId = new QLineEdit(m_game.fakeSteamId);
    m_fakeSteamId->setPlaceholderText(QStringLiteral("Usually 480"));
    identityForm->addRow(QStringLiteral("Steam overlay AppID"), m_fakeSteamId);

    generalLayout->addWidget(identity);

    auto *artwork = new QGroupBox(QStringLiteral("Artwork"));
    auto *artworkLayout = new QVBoxLayout(artwork);
    artworkLayout->setSpacing(12);

    m_bannerPreview = new QLabel;
    m_bannerPreview->setFixedSize(430, 162);
    m_bannerPreview->setAlignment(Qt::AlignCenter);
    m_bannerPreview->setStyleSheet(QStringLiteral(
        "background: #17171e; border: 1px solid #2e2e3a; border-radius: 10px;"));
    if (!m_bannerPath.isEmpty())
        m_bannerPreview->setPixmap(QPixmap(m_bannerPath).scaled(
            430, 162, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    artworkLayout->addWidget(m_bannerPreview);

    auto *bannerButtons = new QHBoxLayout;
    auto *steamCover = new QPushButton(QStringLiteral("Cover from Steam…"));
    steamCover->setIcon(Icons::icon(Icons::Name::Download, Theme::TextMuted, 16));
    connect(steamCover, &QPushButton::clicked, this, &GameSettingsDialog::changeBannerFromSteam);
    auto *fileCover = new QPushButton(QStringLiteral("Cover from file…"));
    fileCover->setIcon(Icons::icon(Icons::Name::Folder, Theme::TextMuted, 16));
    connect(fileCover, &QPushButton::clicked, this, &GameSettingsDialog::changeBannerFromFile);
    bannerButtons->addWidget(steamCover);
    bannerButtons->addWidget(fileCover);
    bannerButtons->addStretch();
    artworkLayout->addLayout(bannerButtons);

    auto *iconRow = new QHBoxLayout;
    m_iconPreview = new QLabel;
    m_iconPreview->setFixedSize(64, 64);
    m_iconPreview->setAlignment(Qt::AlignCenter);
    m_iconPreview->setStyleSheet(QStringLiteral("background: #17171e; border-radius: 14px;"));
    if (!m_iconPath.isEmpty()) {
        const QPixmap icon(m_iconPath);
        if (!icon.isNull())
            m_iconPreview->setPixmap(icon.scaled(64, 64, Qt::KeepAspectRatioByExpanding,
                                                 Qt::SmoothTransformation));
    }
    iconRow->addWidget(m_iconPreview);
    auto *iconButton = new QPushButton(QStringLiteral("Change icon…"));
    iconButton->setIcon(Icons::icon(Icons::Name::Edit, Theme::TextMuted, 16));
    connect(iconButton, &QPushButton::clicked, this, &GameSettingsDialog::changeIcon);
    iconRow->addWidget(iconButton);
    iconRow->addStretch();
    artworkLayout->addLayout(iconRow);

    generalLayout->addWidget(artwork);
    generalLayout->addStretch();
    generalScroll->setWidget(general);
    tabs->addTab(generalScroll, QStringLiteral("General"));

    // ---- Launch -------------------------------------------------------
    auto *launchScroll = new QScrollArea;
    launchScroll->setWidgetResizable(true);
    auto *launch = new QWidget;
    auto *launchLayout = new QVBoxLayout(launch);
    launchLayout->setContentsMargins(2, 12, 12, 12);
    launchLayout->setSpacing(16);

    auto *protonGroup = new QGroupBox(QStringLiteral("Proton"));
    auto *protonForm = new QFormLayout(protonGroup);
    m_proton = new QComboBox;
    connect(m_proton, &QComboBox::activated, this, [this](int) {
        if (m_proton->currentData().toString() == QLatin1String("__download__")) {
            const QString previous = m_game.proton;
            const int previousIndex = m_proton->findText(previous);
            m_proton->setCurrentIndex(previousIndex >= 0 ? previousIndex : 0);
            LauncherSettingsDialog dialog(m_store, m_protons, this);
            dialog.showProtonPage();
            dialog.exec();
            reloadProtons();
            const int index = m_proton->findText(previous);
            if (index >= 0)
                m_proton->setCurrentIndex(index);
        }
    });
    protonForm->addRow(QStringLiteral("Version"), m_proton);

    m_fixPathLabel = new QLabel;
    m_fixPathLabel->setObjectName(QStringLiteral("small"));
    m_fixPathLabel->setWordWrap(true);
    m_fixPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    protonForm->addRow(QStringLiteral("Fix folder"), m_fixPathLabel);
    launchLayout->addWidget(protonGroup);
    reloadProtons();

    auto *command = new QGroupBox(QStringLiteral("Command line and environment"));
    auto *commandForm = new QFormLayout(command);
    commandForm->setSpacing(10);
    m_argsBefore = new QLineEdit(m_game.argsBefore);
    m_argsBefore->setPlaceholderText(QStringLiteral("e.g. gamescope -f -W 2560 -H 1440 --"));
    commandForm->addRow(QStringLiteral("Arguments before"), m_argsBefore);
    m_argsAfter = new QLineEdit(m_game.argsAfter);
    m_argsAfter->setPlaceholderText(QStringLiteral("Passed after the executable"));
    commandForm->addRow(QStringLiteral("Arguments after"), m_argsAfter);

    auto *envRow = new QWidget;
    auto *envLayout = new QHBoxLayout(envRow);
    envLayout->setContentsMargins(0, 0, 0, 0);
    envLayout->setSpacing(8);
    m_environmentLabel = new QLabel;
    m_environmentLabel->setObjectName(QStringLiteral("muted"));
    envLayout->addWidget(m_environmentLabel, 1);
    auto *envButton = new QPushButton(QStringLiteral("Edit…"));
    envButton->setIcon(Icons::icon(Icons::Name::Edit, Theme::TextMuted, 16));
    connect(envButton, &QPushButton::clicked, this, [this] {
        EnvDialog dialog(m_environment, m_overlay->isChecked(), this);
        if (dialog.exec() == QDialog::Accepted) {
            m_environment = dialog.environment();
            updateEnvironmentLabel();
        }
    });
    envLayout->addWidget(envButton);
    commandForm->addRow(QStringLiteral("Environment"), envRow);

    m_overrides = new QLineEdit(m_game.overrides);
    m_overrides->setPlaceholderText(QStringLiteral("e.g. onlinefix64=n;steam_api64=n;winmm=n,b"));
    commandForm->addRow(QStringLiteral("DLL overrides"), m_overrides);
    launchLayout->addWidget(command);

    auto *compat = new QGroupBox(QStringLiteral("Compatibility"));
    auto *compatLayout = new QVBoxLayout(compat);
    compatLayout->setSpacing(9);

    m_overlay = new QCheckBox(QStringLiteral("Steam overlay"));
    m_overlay->setChecked(m_game.steamOverlay);
    compatLayout->addWidget(m_overlay);
    compatLayout->addWidget(hintLabel(QStringLiteral(
        "Injects Steam's overlay renderer and reports the overlay AppID (usually 480).")));

    m_runtime = new QCheckBox(QStringLiteral("Steam Linux Runtime"));
    m_runtime->setChecked(m_game.steamRuntime);
    compatLayout->addWidget(m_runtime);
    compatLayout->addWidget(hintLabel(QStringLiteral(
        "Runs Proton inside Valve's container runtime. Recommended for GE-Proton 8 and newer.")));

    m_wayland = new QCheckBox(QStringLiteral("Native Wayland"));
    m_wayland->setChecked(m_game.nativeWayland);
    compatLayout->addWidget(m_wayland);
    compatLayout->addWidget(hintLabel(QStringLiteral(
        "Sets PROTON_ENABLE_WAYLAND=1. Useful on Wayland desktops; some games may not support it.")));

    m_wined3d = new QCheckBox(QStringLiteral("Use WINED3D instead of DXVK"));
    m_wined3d->setChecked(m_game.wined3d);
    compatLayout->addWidget(m_wined3d);
    compatLayout->addWidget(hintLabel(QStringLiteral(
        "Disables DXVK by overriding the Direct3D DLLs. Only enable it for games with rendering issues.")));

    m_bypass = new QCheckBox(QStringLiteral("Bypass «Steam not running»"));
    m_bypass->setChecked(m_originalBypass);
    compatLayout->addWidget(m_bypass);
    compatLayout->addWidget(hintLabel(QStringLiteral(
        "Replaces steamfix32/64.dll with a stub that works without Steam. "
        "Only available for FreeTP-style fixes.")));

    launchLayout->addWidget(compat);
    launchLayout->addStretch();
    launchScroll->setWidget(launch);
    tabs->addTab(launchScroll, QStringLiteral("Launch"));

    layout->addWidget(tabs, 1);

    m_error = new QLabel;
    m_error->setObjectName(QStringLiteral("error"));
    m_error->setWordWrap(true);
    layout->addWidget(m_error);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("Save changes"));
    buttons->button(QDialogButtonBox::Save)->setObjectName(QStringLiteral("primary"));
    connect(buttons, &QDialogButtonBox::accepted, this, &GameSettingsDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    m_fixPathLabel->setText(m_game.fixPath.isEmpty() ? QStringLiteral("No fix files detected")
                                                      : m_game.fixPath);
    updateEnvironmentLabel();
    updateBypassAvailability();
    m_executable->setCursorPosition(0);
    m_mainPath->setCursorPosition(0);
    m_prefixPath->setCursorPosition(0);
}

void GameSettingsDialog::reloadProtons()
{
    const QString current = m_proton->currentText().isEmpty() ? m_game.proton
                                                              : m_proton->currentText();
    m_proton->clear();
    m_proton->addItem(QStringLiteral("GE-Proton Latest"));
    for (const QString &name : m_protons->installed())
        m_proton->addItem(name);
    if (!current.isEmpty() && m_proton->findText(current) < 0)
        m_proton->addItem(current);
    m_proton->insertSeparator(m_proton->count());
    m_proton->addItem(QStringLiteral("Download more…"), QStringLiteral("__download__"));
    const int index = m_proton->findText(current);
    m_proton->setCurrentIndex(index >= 0 ? index : 0);
}

void GameSettingsDialog::updateEnvironmentLabel()
{
    const int count = m_environment.size();
    m_environmentLabel->setText(count == 0 ? QStringLiteral("No extra variables")
                                           : QStringLiteral("%1 variable(s) configured").arg(count));
}

void GameSettingsDialog::changeBannerFromFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Choose a cover"), QDir::homePath(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
    if (path.isEmpty())
        return;

    const QString suffix = QFileInfo(path).suffix().toLower();
    const QString destination = uniqueBannerPath(m_name->text().trimmed(), suffix);
    if (!QFile::copy(path, destination)) {
        QMessageBox::critical(this, QStringLiteral("Cover"), QStringLiteral("Could not copy the image."));
        return;
    }
    m_bannerPath = destination;
    m_bannerPreview->setPixmap(QPixmap(destination).scaled(
        430, 162, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
}

void GameSettingsDialog::changeBannerFromSteam()
{
    bool ok = false;
    const QString appId = QInputDialog::getText(
        this, QStringLiteral("Cover from Steam"), QStringLiteral("Steam AppID:"),
        QLineEdit::Normal, m_steamId->text(), &ok);
    if (!ok || appId.trimmed().isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const QString banner = FixParser::fetchBanner(appId.trimmed(), &error);
    QApplication::restoreOverrideCursor();
    if (banner.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("Cover"), error);
        return;
    }
    m_bannerPath = banner;
    m_steamId->setText(appId.trimmed());
    m_bannerPreview->setPixmap(QPixmap(banner).scaled(
        430, 162, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
}

void GameSettingsDialog::changeIcon()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Choose an icon or executable"), QDir::homePath(),
        QStringLiteral("Images and executables (*.png *.jpg *.jpeg *.webp *.ico *.exe);;All files (*)"));
    if (path.isEmpty())
        return;

    QString icon;
    QString error;
    if (QFileInfo(path).suffix().compare(QLatin1String("exe"), Qt::CaseInsensitive) == 0)
        icon = IconExtractor::extractFromExecutable(path, &error);
    else
        icon = IconExtractor::copyImage(path, &error);

    if (icon.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("Icon"), error);
        return;
    }
    m_iconPath = icon;
    const QPixmap pixmap(icon);
    if (!pixmap.isNull())
        m_iconPreview->setPixmap(pixmap.scaled(64, 64, Qt::KeepAspectRatioByExpanding,
                                               Qt::SmoothTransformation));
}

void GameSettingsDialog::choosePrefix()
{
    const QString path = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select the prefix folder"),
        m_prefixPath->text().isEmpty() ? Paths::defaultPrefixesDir() : m_prefixPath->text());
    if (path.isEmpty())
        return;
    if (QFileInfo(path).isDir() && !QDir(path).isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Folder is not empty"),
                             QStringLiteral("This prefix folder already contains files. "
                                            "Removing the game may delete them."));
    }
    m_prefixPath->setText(path);
}

void GameSettingsDialog::chooseExecutable()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Select the game executable"),
        m_mainPath->text().isEmpty() ? QDir::homePath() : m_mainPath->text(),
        QStringLiteral("Windows executables (*.exe *.bat *.vbs);;All files (*)"));
    if (!path.isEmpty())
        m_executable->setText(path);
}

void GameSettingsDialog::chooseMainPath()
{
    const QString path = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select the game folder"),
        m_mainPath->text().isEmpty() ? QDir::homePath() : m_mainPath->text());
    if (!path.isEmpty())
        m_mainPath->setText(path);
}

void GameSettingsDialog::updateBypassAvailability()
{
    const bool available = !m_game.fixPath.isEmpty() && QFileInfo(m_game.fixPath).isDir();
    m_bypass->setEnabled(available);
    if (!available) {
        QSignalBlocker blocker(m_bypass);
        m_bypass->setChecked(false);
    }
}

void GameSettingsDialog::save()
{
    m_error->clear();
    const QString newName = m_name->text().trimmed();
    if (newName.isEmpty()) {
        m_error->setText(QStringLiteral("The game name cannot be empty."));
        return;
    }
    if (newName != m_gameName && m_store->hasGame(newName)) {
        m_error->setText(QStringLiteral("A game named \"%1\" already exists.").arg(newName));
        return;
    }
    if (!QFileInfo(m_executable->text().trimmed()).isFile()) {
        m_error->setText(QStringLiteral("The game executable does not exist."));
        return;
    }
    if (m_mainPath->text().trimmed().isEmpty()
        || !QFileInfo(m_mainPath->text().trimmed()).isDir()) {
        m_error->setText(QStringLiteral("The game folder does not exist."));
        return;
    }
    if (m_prefixPath->text().trimmed().isEmpty()) {
        m_error->setText(QStringLiteral("The prefix folder cannot be empty."));
        return;
    }

    // Steam Runtime: refuse to enable it when Valve's runtime is missing.
    bool steamRuntime = m_runtime->isChecked();
    if (steamRuntime) {
        const QString proton = m_protons->resolve(m_proton->currentText());
        if (proton.isEmpty()
            || Steam::findRuntime(m_protons->pathFor(proton)).isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Steam Runtime"),
                                 QStringLiteral("Steam Linux Runtime is not installed for this Proton version. "
                                                "It will be disabled."));
            steamRuntime = false;
        }
    }

    // Move the prefix when the user changed the path.
    const QString newPrefix = m_prefixPath->text().trimmed();
    if (newPrefix != m_originalPrefix && !m_originalPrefix.isEmpty()
        && QFileInfo(m_originalPrefix).isDir() && !QDir(m_originalPrefix).isEmpty()) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Move the prefix?"),
            QStringLiteral("Move the existing prefix to the new folder?\n\nFrom: %1\nTo: %2")
                .arg(m_originalPrefix, newPrefix));
        if (answer == QMessageBox::Yes) {
            QApplication::setOverrideCursor(Qt::WaitCursor);
            QString moveError;
            const bool moved = Utils::moveDirectoryContents(m_originalPrefix, newPrefix, &moveError);
            QApplication::restoreOverrideCursor();
            if (!moved) {
                m_error->setText(QStringLiteral("Could not move the prefix: %1").arg(moveError));
                return;
            }
        }
    }

    // Apply or remove the FreeTP "no Steam" patch.
    const bool newBypass = m_bypass->isEnabled() && m_bypass->isChecked();
    if (newBypass != m_originalBypass) {
        QString patchError;
        if (!FixParser::setNoOfllPath(m_game.fixPath, newBypass, &patchError)) {
            m_error->setText(patchError);
            return;
        }
    }

    Game updated = m_game;
    updated.name = newName;
    updated.executable = m_executable->text().trimmed();
    updated.mainPath = m_mainPath->text().trimmed();
    updated.prefixPath = newPrefix;
    updated.proton = m_proton->currentData().toString() == QLatin1String("__download__")
        ? m_game.proton
        : m_proton->currentText();
    updated.steamId = m_steamId->text().trimmed();
    updated.fakeSteamId = m_fakeSteamId->text().trimmed();
    updated.argsBefore = m_argsBefore->text();
    updated.argsAfter = m_argsAfter->text();
    updated.overrides = m_overrides->text().trimmed();
    updated.environment = m_environment;
    updated.banner = m_bannerPath;
    updated.icon = m_iconPath;
    updated.steamOverlay = m_overlay->isChecked();
    updated.steamRuntime = steamRuntime;
    updated.nativeWayland = m_wayland->isChecked();
    updated.wined3d = m_wined3d->isChecked();

    QString error;
    if (newName != m_gameName) {
        if (!m_store->renameGame(m_gameName, newName, &error)) {
            m_error->setText(error);
            return;
        }
    }
    if (!m_store->saveGame(updated, &error)) {
        m_error->setText(error);
        return;
    }

    // Refresh desktop entries (rename and/or new icon).
    const QString desktop = Paths::desktopDir();
    const QString menu = Paths::applicationsDir();
    const bool hadDesktop = DesktopEntry::exists(desktop, m_gameName);
    const bool hadMenu = DesktopEntry::exists(menu, m_gameName);
    if (hadDesktop)
        DesktopEntry::remove(desktop, m_gameName);
    if (hadMenu)
        DesktopEntry::remove(menu, m_gameName);
    if (hadDesktop)
        DesktopEntry::create(desktop, newName, m_iconPath);
    if (hadMenu)
        DesktopEntry::create(menu, newName, m_iconPath);

    const auto usedElsewhere = [this](const QString &path, const QString &except) {
        if (path.isEmpty())
            return false;
        for (const QString &other : m_store->gameNames()) {
            if (other == except)
                continue;
            const Game otherGame = m_store->game(other);
            if (otherGame.banner == path || otherGame.icon == path)
                return true;
        }
        return false;
    };

    if (m_originalBanner != m_bannerPath && !m_originalBanner.isEmpty()
        && QFileInfo(m_originalBanner).isFile()
        && !usedElsewhere(m_originalBanner, newName))
        QFile::remove(m_originalBanner);
    if (m_originalIcon != m_iconPath && !m_originalIcon.isEmpty()
        && QFileInfo(m_originalIcon).isFile()
        && !usedElsewhere(m_originalIcon, newName))
        QFile::remove(m_originalIcon);

    m_gameName = newName;
    emit saved();
    accept();
}
