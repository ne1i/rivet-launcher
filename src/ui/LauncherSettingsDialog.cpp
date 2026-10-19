#include "LauncherSettingsDialog.h"

#include "Icons.h"
#include "Paths.h"
#include "Steam.h"
#include "Theme.h"
#include "Utils.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace {

QWidget *pathRow(const QString &title, const QString &hint, QLineEdit *edit,
                 const QString &placeholder, const QString &defaultValue,
                 QWidget *parent)
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("settingRow"));
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(4);

    auto *titleLabel = new QLabel(title);
    titleLabel->setStyleSheet(QStringLiteral("font-weight: 600;"));
    layout->addWidget(titleLabel);

    auto *hintLabel = new QLabel(hint);
    hintLabel->setObjectName(QStringLiteral("settingHint"));
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    auto *row = new QHBoxLayout;
    row->setSpacing(8);
    edit->setPlaceholderText(placeholder);
    edit->setText(defaultValue);
    row->addWidget(edit, 1);

    auto *browse = new QPushButton(QStringLiteral("Browse…"));
    QObject::connect(browse, &QPushButton::clicked, edit, [edit, title] {
        const QString path = QFileDialog::getExistingDirectory(
            edit->window(), title, edit->text().isEmpty() ? QDir::homePath() : edit->text());
        if (!path.isEmpty())
            edit->setText(path);
    });
    row->addWidget(browse);

    auto *reset = new QPushButton(QStringLiteral("Reset"));
    reset->setObjectName(QStringLiteral("ghost"));
    reset->setToolTip(QStringLiteral("Use the default path"));
    QObject::connect(reset, &QPushButton::clicked, edit,
                     [edit, defaultValue] { edit->setText(defaultValue); });
    row->addWidget(reset);
    layout->addLayout(row);

    Q_UNUSED(parent);
    return frame;
}

QLabel *statusLabel(bool ok, const QString &okText, const QString &missingText)
{
    auto *label = new QLabel(ok ? okText : missingText);
    label->setObjectName(ok ? QStringLiteral("success") : QStringLiteral("error"));
    return label;
}

} // namespace

LauncherSettingsDialog::LauncherSettingsDialog(Config *store, ProtonManager *protons,
                                               QWidget *parent)
    : QDialog(parent)
    , m_store(store)
    , m_protons(protons)
{
    setWindowTitle(QStringLiteral("Launcher settings"));
    setMinimumSize(900, 650);
    resize(940, 700);
    buildUi();

    m_fetchWatcher = new QFutureWatcher<ProtonFetchResult>(this);
    connect(m_fetchWatcher, &QFutureWatcher<ProtonFetchResult>::finished, this, [this] {
        m_fetching = false;
        if (m_refreshProtons)
            m_refreshProtons->setEnabled(true);
        const ProtonFetchResult result = m_fetchWatcher->result();
        if (!result.error.isEmpty()) {
            m_protonStatus->setText(QStringLiteral("Could not load releases: %1").arg(result.error));
            m_protonStatus->setObjectName(QStringLiteral("error"));
        } else {
            m_releases = result.releases;
            m_protonStatus->setText(QStringLiteral("%1 releases available from GitHub").arg(m_releases.size()));
            m_protonStatus->setObjectName(QStringLiteral("muted"));
        }
        m_protonStatus->style()->unpolish(m_protonStatus);
        m_protonStatus->style()->polish(m_protonStatus);
        reloadProtonPage();
    });

    m_downloadWatcher = new QFutureWatcher<QString>(this);
    connect(m_downloadWatcher, &QFutureWatcher<QString>::finished, this, [this] {
        m_downloading = false;
        if (m_downloadDialog) {
            m_downloadDialog->close();
            m_downloadDialog->deleteLater();
            m_downloadDialog = nullptr;
        }
        const QString error = m_downloadWatcher->result();
        if (!error.isEmpty())
            QMessageBox::critical(this, QStringLiteral("Proton"), error);
        else
            m_protonStatus->setText(QStringLiteral("Installed %1").arg(m_downloadingName));
        refillDefaultProton();
        reloadProtonPage();
        fetchReleases();
    });

    reloadProtonPage();
    fetchReleases();
}

void LauncherSettingsDialog::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 18);
    layout->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("Launcher settings"));
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);

    auto *content = new QHBoxLayout;
    content->setSpacing(22);

    m_navigation = new QListWidget;
    m_navigation->setFixedWidth(190);
    m_navigation->setIconSize(QSize(18, 18));
    m_navigation->setSpacing(2);
    auto addPage = [this](const QString &label, Icons::Name icon) {
        auto *item = new QListWidgetItem(Icons::icon(icon, Theme::TextMuted, 18), label);
        item->setSizeHint(QSize(0, 42));
        m_navigation->addItem(item);
    };
    addPage(QStringLiteral("Paths"), Icons::Name::Folder);
    addPage(QStringLiteral("Downloads"), Icons::Name::Download);
    addPage(QStringLiteral("Proton"), Icons::Name::Wine);
    addPage(QStringLiteral("Launcher"), Icons::Name::Monitor);
    addPage(QStringLiteral("About"), Icons::Name::Info);
    content->addWidget(m_navigation);

    m_pages = new QStackedWidget;
    m_pages->addWidget(buildPathsPage());
    m_pages->addWidget(buildDownloadsPage());
    m_pages->addWidget(buildProtonPage());
    m_pages->addWidget(buildLauncherPage());
    m_pages->addWidget(buildAboutPage());
    content->addWidget(m_pages, 1);

    connect(m_navigation, &QListWidget::currentRowChanged, m_pages,
            &QStackedWidget::setCurrentIndex);
    m_navigation->setCurrentRow(0);

    layout->addLayout(content, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("Save settings"));
    buttons->button(QDialogButtonBox::Save)->setObjectName(QStringLiteral("primary"));
    connect(buttons, &QDialogButtonBox::accepted, this, &LauncherSettingsDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QWidget *LauncherSettingsDialog::buildPathsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(2, 4, 10, 4);
    layout->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("Default folders"));
    title->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(title);
    auto *hint = new QLabel(QStringLiteral(
        "These are the defaults used when adding games. Per-game settings can override them."));
    hint->setObjectName(QStringLiteral("muted"));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_downloads = new QLineEdit;
    m_installs = new QLineEdit;
    m_prefixes = new QLineEdit;
    m_protonsPath = new QLineEdit;

    layout->addWidget(pathRow(
        QStringLiteral("Game downloads"),
        QStringLiteral("Where downloaded archives are kept."),
        m_downloads, m_store->downloadsPath(), m_store->downloadsPath(), this));
    layout->addWidget(pathRow(
        QStringLiteral("Game installs"),
        QStringLiteral("Where archives, installers and new games are installed. "
                       "Leave empty to ask every time."),
        m_installs, QStringLiteral("Ask every time"), m_store->installsPath(), this));
    layout->addWidget(pathRow(
        QStringLiteral("Prefixes"),
        QStringLiteral("Wine/Proton prefixes for the games."),
        m_prefixes, Paths::defaultPrefixesDir(), m_store->prefixesPath(), this));
    layout->addWidget(pathRow(
        QStringLiteral("Proton builds"),
        QStringLiteral("Downloaded GE-Proton builds. You can also drop your own builds in this folder."),
        m_protonsPath, Paths::defaultProtonsDir(), m_store->protonsPath(), this));

    layout->addStretch();

    connect(m_protonsPath, &QLineEdit::textChanged, this, [this] {
        // Saving the path is what changes the Proton manager; refresh after save.
    });

    return page;
}

QWidget *LauncherSettingsDialog::buildDownloadsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(2, 4, 10, 4);
    layout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Game downloads"));
    title->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(title);

    auto *hint = new QLabel(QStringLiteral(
        "The download page reads a Hydra/OnlineFix JSON source. "
        "Paste the source URL here, then use “Download games” in the main window."));
    hint->setObjectName(QStringLiteral("muted"));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *sourceFrame = new QFrame;
    sourceFrame->setObjectName(QStringLiteral("settingRow"));
    auto *sourceLayout = new QVBoxLayout(sourceFrame);
    sourceLayout->setContentsMargins(14, 12, 14, 12);
    sourceLayout->setSpacing(6);
    auto *sourceTitle = new QLabel(QStringLiteral("Hydra source"));
    sourceTitle->setStyleSheet(QStringLiteral("font-weight: 600;"));
    sourceLayout->addWidget(sourceTitle);
    auto *sourceHint = new QLabel(QStringLiteral(
        "Example: https://example.com/onlinefix.json. The source must contain "
        "a downloads list with title, fileSize and uris."));
    sourceHint->setObjectName(QStringLiteral("settingHint"));
    sourceHint->setWordWrap(true);
    sourceLayout->addWidget(sourceHint);

    auto *sourceRow = new QHBoxLayout;
    m_downloadSource = new QLineEdit(m_store->downloadsSource());
    m_downloadSource->setPlaceholderText(QStringLiteral("https://…/onlinefix.json"));
    sourceRow->addWidget(m_downloadSource, 1);
    auto *clear = new QPushButton(QStringLiteral("Clear"));
    clear->setObjectName(QStringLiteral("ghost"));
    connect(clear, &QPushButton::clicked, m_downloadSource, &QLineEdit::clear);
    sourceRow->addWidget(clear);
    sourceLayout->addLayout(sourceRow);
    layout->addWidget(sourceFrame);

    auto *folderFrame = new QFrame;
    folderFrame->setObjectName(QStringLiteral("settingRow"));
    auto *folderLayout = new QHBoxLayout(folderFrame);
    folderLayout->setContentsMargins(14, 12, 14, 12);
    folderLayout->setSpacing(12);
    auto *folderTitle = new QLabel(QStringLiteral("Download folder"));
    folderTitle->setStyleSheet(QStringLiteral("font-weight: 600;"));
    folderLayout->addWidget(folderTitle);
    auto *folderValue = new QLabel(m_downloads ? m_downloads->text()
                                               : m_store->downloadsPath());
    folderValue->setObjectName(QStringLiteral("muted"));
    folderValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    folderLayout->addWidget(folderValue, 1);
    connect(m_downloads, &QLineEdit::textChanged, folderValue, &QLabel::setText);
    auto *openFolder = new QPushButton(QStringLiteral("Open"));
    connect(openFolder, &QPushButton::clicked, this, [this] {
        const QString path = m_downloads->text().trimmed().isEmpty()
            ? Paths::downloadsDir()
            : m_downloads->text().trimmed();
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    folderLayout->addWidget(openFolder);
    layout->addWidget(folderFrame);

    auto *aria = new QLabel(
        Utils::which(QStringLiteral("aria2c"))
            ? QStringLiteral("aria2c: found")
            : QStringLiteral("aria2c: missing — install the aria2 package to download games."));
    aria->setObjectName(Utils::which(QStringLiteral("aria2c")) ? QStringLiteral("success")
                                                               : QStringLiteral("error"));
    layout->addWidget(aria);

    layout->addStretch();
    return page;
}

QWidget *LauncherSettingsDialog::buildProtonPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(2, 4, 10, 4);
    layout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Proton builds"));
    title->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(title);

    auto *form = new QFormLayout;
    m_defaultProton = new QComboBox;
    form->addRow(QStringLiteral("Default for new games"), m_defaultProton);
    layout->addLayout(form);

    auto *hint = new QLabel(QStringLiteral(
        "Games use the selected Proton build. \"GE-Proton Latest\" resolves to the newest build installed here."));
    hint->setObjectName(QStringLiteral("muted"));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *header = new QHBoxLayout;
    auto *listTitle = new QLabel(QStringLiteral("Installed and available"));
    listTitle->setStyleSheet(QStringLiteral("font-weight: 600;"));
    header->addWidget(listTitle);
    header->addStretch();
    m_protonStatus = new QLabel;
    m_protonStatus->setObjectName(QStringLiteral("muted"));
    header->addWidget(m_protonStatus);
    m_refreshProtons = new QPushButton(QStringLiteral("Refresh"));
    m_refreshProtons->setIcon(Icons::icon(Icons::Name::Refresh, Theme::TextMuted, 16));
    connect(m_refreshProtons, &QPushButton::clicked, this, &LauncherSettingsDialog::fetchReleases);
    header->addWidget(m_refreshProtons);
    layout->addLayout(header);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    m_protonListHost = new QWidget;
    m_protonList = new QVBoxLayout(m_protonListHost);
    m_protonList->setContentsMargins(0, 0, 6, 0);
    m_protonList->setSpacing(8);
    m_protonList->addStretch();
    scroll->setWidget(m_protonListHost);
    layout->addWidget(scroll, 1);

    return page;
}

QWidget *LauncherSettingsDialog::buildLauncherPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(2, 4, 10, 4);
    layout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Launcher behaviour"));
    title->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(title);

    m_fullscreen = new QCheckBox(QStringLiteral("Start the launcher fullscreen"));
    m_fullscreen->setChecked(m_store->fullscreen());
    layout->addWidget(m_fullscreen);

    m_noSteam = new QCheckBox(QStringLiteral("Do not require Steam to be running"));
    m_noSteam->setChecked(m_store->noSteamRequest());
    layout->addWidget(m_noSteam);
    auto *steamHint = new QLabel(QStringLiteral(
        "By default the launcher starts Steam and waits for you to sign in. "
        "Enable this if your fixes do not need Steam at all."));
    steamHint->setObjectName(QStringLiteral("small"));
    steamHint->setWordWrap(true);
    steamHint->setContentsMargins(26, 0, 0, 0);
    layout->addWidget(steamHint);

    auto *sep = new QFrame;
    sep->setObjectName(QStringLiteral("separator"));
    layout->addWidget(sep);

    auto *graphics = new QLabel(QStringLiteral("Defaults for newly added games"));
    graphics->setStyleSheet(QStringLiteral("font-weight: 600;"));
    layout->addWidget(graphics);

    m_waylandDefault = new QCheckBox(QStringLiteral("Enable native Wayland"));
    m_waylandDefault->setChecked(m_store->gamesUseWayland());
    layout->addWidget(m_waylandDefault);

    m_wined3dDefault = new QCheckBox(QStringLiteral("Use WINED3D instead of DXVK"));
    m_wined3dDefault->setChecked(m_store->gamesUseWined3d());
    layout->addWidget(m_wined3dDefault);

    auto *graphicsHint = new QLabel(QStringLiteral(
        "These only affect games added from now on. Existing games keep their settings."));
    graphicsHint->setObjectName(QStringLiteral("small"));
    graphicsHint->setWordWrap(true);
    graphicsHint->setContentsMargins(26, 0, 0, 0);
    layout->addWidget(graphicsHint);

    layout->addStretch();
    return page;
}

QWidget *LauncherSettingsDialog::buildAboutPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(2, 4, 10, 4);
    layout->setSpacing(10);

    auto *header = new QHBoxLayout;
    auto *icon = new QLabel;
    icon->setPixmap(Icons::appIcon().pixmap(72, 72));
    header->addWidget(icon);
    auto *titles = new QVBoxLayout;
    auto *title = new QLabel(QStringLiteral("Rivet"));
    title->setObjectName(QStringLiteral("pageTitle"));
    auto *version = new QLabel(QStringLiteral("Version %1 — native Qt 6 rewrite")
                                   .arg(QCoreApplication::applicationVersion()));
    version->setObjectName(QStringLiteral("muted"));
    titles->addWidget(title);
    titles->addWidget(version);
    header->addLayout(titles, 1);
    layout->addLayout(header);

    auto *description = new QLabel(QStringLiteral(
        "Rivet is a native Linux launcher for games using OnlineFix, FreeTP and custom multiplayer fixes. "
        "It configures WINEDLLOVERRIDES, Steam overlay, runtime and the game prefix for you.\n\n"
        "Licensed under the GNU AGPL-3.0. Based on the original onlinefix-linux project by ZzEdovec."));
    description->setWordWrap(true);
    description->setObjectName(QStringLiteral("muted"));
    layout->addWidget(description);

    auto *links = new QHBoxLayout;
    auto *github = new QPushButton(QStringLiteral("Original project on GitHub"));
    github->setIcon(Icons::icon(Icons::Name::External, Theme::TextMuted, 16));
    connect(github, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/ZzEdovec/onlinefix-linux")));
    });
    auto *config = new QPushButton(QStringLiteral("Open config folder"));
    config->setIcon(Icons::icon(Icons::Name::Folder, Theme::TextMuted, 16));
    connect(config, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(Paths::configDir()));
    });
    links->addWidget(github);
    links->addWidget(config);
    links->addStretch();
    layout->addLayout(links);

    auto *depsTitle = new QLabel(QStringLiteral("Dependencies"));
    depsTitle->setStyleSheet(QStringLiteral("font-weight: 600;"));
    layout->addWidget(depsTitle);

    const struct {
        const char *name;
        const char *binary;
        bool optional;
    } dependencies[] = {
        {"Steam (native package)", "steam", false},
        {"FFmpeg", "ffmpeg", true},
        {"icoextract", "icoextract", true},
        {"unrar", "unrar", false},
        {"7-Zip", "7z", false},
        {"aria2c", "aria2c", true},
        {"tar", "tar", false},
        {"Winetricks", "winetricks", true},
    };

    for (const auto &dependency : dependencies) {
        auto *row = new QHBoxLayout;
        auto *name = new QLabel(QString::fromLatin1(dependency.name));
        name->setMinimumWidth(230);
        row->addWidget(name);
        const bool found = Utils::which(QString::fromLatin1(dependency.binary));
        auto *status = statusLabel(found,
                                   dependency.optional ? QStringLiteral("Found (optional)")
                                                       : QStringLiteral("Found"),
                                   dependency.optional ? QStringLiteral("Not installed (optional)")
                                                       : QStringLiteral("Missing"));
        row->addWidget(status);
        row->addStretch();
        layout->addLayout(row);
    }

    auto *note = new QLabel(QStringLiteral(
        "Steam must be the native package. Flatpak and Snap builds are not supported by Proton prefixes."));
    note->setObjectName(QStringLiteral("small"));
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch();
    return page;
}

void LauncherSettingsDialog::browsePath(QLineEdit *edit, const QString &title, const QString &start)
{
    const QString path = QFileDialog::getExistingDirectory(this, title, start);
    if (!path.isEmpty())
        edit->setText(path);
}

void LauncherSettingsDialog::reloadProtonPage()
{
    if (!m_protonList)
        return;

    // Clear rows.
    while (QLayoutItem *item = m_protonList->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    const QStringList installed = m_protons->installed();
    const QStringList allInstalled = m_protons->installedAll();
    for (const QString &name : allInstalled) {
        const bool compatible = installed.contains(name);
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("settingRow"));
        auto *row = new QHBoxLayout(frame);
        row->setContentsMargins(14, 10, 14, 10);
        row->setSpacing(12);
        auto *label = new QLabel(name);
        label->setStyleSheet(QStringLiteral("font-weight: 600;"));
        row->addWidget(label, 1);
        auto *status = new QLabel(compatible ? QStringLiteral("Installed")
                                             : QStringLiteral("Incompatible architecture"));
        status->setObjectName(compatible ? QStringLiteral("success") : QStringLiteral("error"));
        row->addWidget(status);
        auto *remove = new QPushButton(QStringLiteral("Remove"));
        remove->setObjectName(QStringLiteral("danger"));
        remove->setEnabled(!m_downloading);
        connect(remove, &QPushButton::clicked, this,
                [this, name] { removeProton(name); });
        row->addWidget(remove);
        m_protonList->addWidget(frame);
    }

    for (const ProtonRelease &release : m_releases) {
        if (allInstalled.contains(release.name))
            continue;
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("settingRow"));
        auto *row = new QHBoxLayout(frame);
        row->setContentsMargins(14, 10, 14, 10);
        row->setSpacing(12);
        auto *label = new QLabel(release.name);
        label->setStyleSheet(QStringLiteral("font-weight: 600;"));
        row->addWidget(label, 1);
        auto *sizeHint = new QLabel(release.filename);
        sizeHint->setObjectName(QStringLiteral("small"));
        sizeHint->setToolTip(release.url);
        row->addWidget(sizeHint);
        auto *download = new QPushButton(QStringLiteral("Install"));
        download->setIcon(Icons::icon(Icons::Name::Download, Theme::TextMuted, 16));
        download->setEnabled(!m_downloading);
        connect(download, &QPushButton::clicked, this,
                [this, release] { downloadProton(release); });
        row->addWidget(download);
        m_protonList->addWidget(frame);
    }

    if (allInstalled.isEmpty() && m_releases.isEmpty()) {
        auto *empty = new QLabel(m_fetching ? QStringLiteral("Loading releases…")
                                            : QStringLiteral("No Proton builds found. Click Refresh."));
        empty->setObjectName(QStringLiteral("muted"));
        empty->setAlignment(Qt::AlignCenter);
        empty->setMinimumHeight(80);
        m_protonList->addWidget(empty);
    }

    m_protonList->addStretch();
    refillDefaultProton();
}

void LauncherSettingsDialog::fetchReleases()
{
    if (m_fetching || m_downloading)
        return;
    m_fetching = true;
    if (m_refreshProtons)
        m_refreshProtons->setEnabled(false);
    if (m_protonStatus)
        m_protonStatus->setText(QStringLiteral("Loading releases…"));
    m_fetchWatcher->setFuture(QtConcurrent::run([protons = m_protons] {
        ProtonFetchResult result;
        result.releases = protons->fetchReleases(&result.error);
        return result;
    }));
}

void LauncherSettingsDialog::downloadProton(const ProtonRelease &release)
{
    if (m_downloading)
        return;
    m_downloading = true;
    m_downloadingName = release.name;

    m_downloadDialog = new QProgressDialog(
        QStringLiteral("Downloading %1…").arg(release.name), QStringLiteral("Cancel"), 0, 100, this);
    m_downloadDialog->setValue(0);
    m_downloadDialog->setWindowTitle(QStringLiteral("Installing Proton"));
    m_downloadDialog->setWindowModality(Qt::WindowModal);
    m_downloadDialog->setMinimumDuration(0);
    m_downloadDialog->setAutoClose(false);
    m_downloadDialog->setAutoReset(false);
    m_downloadDialog->setCancelButton(nullptr);
    m_downloadDialog->show();

    reloadProtonPage();

    m_downloadWatcher->setFuture(QtConcurrent::run([protons = m_protons, release, guard = QPointer<LauncherSettingsDialog>(this)]() -> QString {
        QString error;
        const bool ok = protons->download(
            release,
            [guard](qint64 received, qint64 total, const QString &phase) {
                if (!guard)
                    return;
                QMetaObject::invokeMethod(
                    guard.data(),
                    [guard, received, total, phase] {
                        if (!guard || !guard->m_downloadDialog)
                            return;
                        guard->m_downloadDialog->setLabelText(phase);
                        if (total > 0) {
                            guard->m_downloadDialog->setRange(0, 100);
                            guard->m_downloadDialog->setValue(
                                int((double(received) / double(total)) * 100.0));
                        } else {
                            guard->m_downloadDialog->setValue(100);
                        }
                    },
                    Qt::QueuedConnection);
            },
            &error);
        return ok ? QString() : error;
    }));
}

void LauncherSettingsDialog::removeProton(const QString &name)
{
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Remove Proton"),
        QStringLiteral("Remove %1 from disk?\n\nAny game using it will need another Proton build.")
            .arg(name));
    if (answer != QMessageBox::Yes)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool removed = m_protons->remove(name, &error);
    QApplication::restoreOverrideCursor();
    if (!removed) {
        QMessageBox::critical(this, QStringLiteral("Proton"), error);
        return;
    }
    if (m_defaultProton->currentText() == name)
        m_defaultProton->setCurrentText(QStringLiteral("GE-Proton Latest"));
    reloadProtonPage();
    fetchReleases();
}

void LauncherSettingsDialog::refillDefaultProton()
{
    if (!m_defaultProton)
        return;
    const QString requested = m_defaultProton->currentText().isEmpty()
        ? m_store->defaultProton()
        : m_defaultProton->currentText();
    QString current = requested;
    if (!current.isEmpty() && current != QLatin1String("GE-Proton Latest")
        && !m_protons->installed().contains(current))
        current = QStringLiteral("GE-Proton Latest");

    m_defaultProton->blockSignals(true);
    m_defaultProton->clear();
    m_defaultProton->addItem(QStringLiteral("GE-Proton Latest"));
    for (const QString &name : m_protons->installed())
        m_defaultProton->addItem(name);
    if (!current.isEmpty() && m_defaultProton->findText(current) < 0)
        m_defaultProton->addItem(current);
    const int index = m_defaultProton->findText(current);
    m_defaultProton->setCurrentIndex(index >= 0 ? index : 0);
    m_defaultProton->blockSignals(false);
}

void LauncherSettingsDialog::showProtonPage()
{
    m_navigation->setCurrentRow(2);
}

void LauncherSettingsDialog::closeEvent(QCloseEvent *event)
{
    if (m_downloading) {
        QMessageBox::information(this, QStringLiteral("Download in progress"),
                                 QStringLiteral("The Proton download is still running. Please wait."));
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

void LauncherSettingsDialog::reject()
{
    if (m_downloading) {
        QMessageBox::information(this, QStringLiteral("Download in progress"),
                                 QStringLiteral("The Proton download is still running. Please wait."));
        return;
    }
    QDialog::reject();
}

void LauncherSettingsDialog::save()
{
    if (m_downloading) {
        QMessageBox::information(this, QStringLiteral("Download in progress"),
                                 QStringLiteral("Wait for the Proton download to finish."));
        return;
    }
    const QString downloads = m_downloads->text().trimmed().isEmpty()
        ? Paths::downloadsDir()
        : m_downloads->text().trimmed();
    const QString installs = m_installs->text().trimmed();
    const QString prefixes = m_prefixes->text().trimmed().isEmpty()
        ? Paths::defaultPrefixesDir()
        : m_prefixes->text().trimmed();
    const QString protonsPath = m_protonsPath->text().trimmed().isEmpty()
        ? Paths::defaultProtonsDir()
        : m_protonsPath->text().trimmed();

    for (const QString &path : {downloads, prefixes, protonsPath}) {
        if (!path.isEmpty() && !QDir().mkpath(path)) {
            QMessageBox::critical(this, QStringLiteral("Invalid folder"),
                                  QStringLiteral("Could not create:\n%1").arg(path));
            return;
        }
    }
    if (!installs.isEmpty() && !QDir().mkpath(installs)) {
        QMessageBox::critical(this, QStringLiteral("Invalid folder"),
                              QStringLiteral("Could not create:\n%1").arg(installs));
        return;
    }

    m_store->setDownloadsPath(downloads);
    m_store->setInstallsPath(installs);
    m_store->setPrefixesPath(prefixes);
    m_store->setProtonsPath(protonsPath);
    m_store->setDownloadsSource(m_downloadSource->text().trimmed());
    if (m_defaultProton->currentText().isEmpty())
        m_store->setDefaultProton(QStringLiteral("GE-Proton Latest"));
    else
        m_store->setDefaultProton(m_defaultProton->currentText());
    m_store->setFullscreen(m_fullscreen->isChecked());
    m_store->setNoSteamRequest(m_noSteam->isChecked());
    m_store->setGamesUseWayland(m_waylandDefault->isChecked());
    m_store->setGamesUseWined3d(m_wined3dDefault->isChecked());

    QString error;
    if (!m_store->save(&error)) {
        QMessageBox::critical(this, QStringLiteral("Settings"), error);
        return;
    }

    emit settingsSaved();
    accept();
}
