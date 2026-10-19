#include "GameDetailPage.h"

#include "BannerWidget.h"
#include "FlowLayout.h"
#include "Icons.h"
#include "Theme.h"
#include "Utils.h"

#include <QCheckBox>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QPixmap roundedPixmap(const QPixmap &source, int size, int radius)
{
    if (source.isNull())
        return {};
    QPixmap result(size, size);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(0, 0, size, size), radius, radius);
    painter.setClipPath(clip);
    const QPixmap scaled = source.scaled(size, size, Qt::KeepAspectRatioByExpanding,
                                         Qt::SmoothTransformation);
    painter.drawPixmap((size - scaled.width()) / 2, (size - scaled.height()) / 2, scaled);
    return result;
}

QFrame *makeInfoCard(const QString &title, const QList<QPair<QString, QString>> &rows)
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("surface"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    auto *titleLabel = new QLabel(title);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(titleLabel);

    for (const auto &row : rows) {
        auto *line = new QWidget;
        auto *lineLayout = new QHBoxLayout(line);
        lineLayout->setContentsMargins(0, 0, 0, 0);
        lineLayout->setSpacing(12);

        auto *key = new QLabel(row.first);
        key->setObjectName(QStringLiteral("muted"));
        key->setMinimumWidth(110);
        key->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        auto *value = new QLabel(row.second.isEmpty() ? QStringLiteral("—") : row.second);
        value->setWordWrap(true);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        value->setToolTip(row.second);
        value->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        lineLayout->addWidget(key, 0);
        lineLayout->addWidget(value, 1);
        layout->addWidget(line);
    }
    layout->addStretch();
    return card;
}

QPushButton *makeActionButton(const QString &text, Icons::Name icon, bool ghost = true)
{
    auto *button = new QPushButton(text);
    button->setIcon(Icons::icon(icon, ghost ? Theme::TextMuted : Theme::Text, 17));
    button->setIconSize(QSize(17, 17));
    if (ghost)
        button->setObjectName(QStringLiteral("ghost"));
    return button;
}

} // namespace

GameDetailPage::GameDetailPage(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(28, 22, 28, 30);
    layout->setSpacing(16);

    m_banner = new BannerWidget;
    layout->addWidget(m_banner);

    // Main action row
    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(10);

    m_icon = new QLabel;
    m_icon->setFixedSize(64, 64);
    m_icon->setStyleSheet(QStringLiteral("background: #1d1d25; border-radius: 14px;"));
    topRow->addWidget(m_icon);

    auto *metaBox = new QVBoxLayout;
    metaBox->setSpacing(3);
    m_meta = new QLabel;
    m_meta->setObjectName(QStringLiteral("muted"));
    m_meta->setWordWrap(true);
    m_meta->setMinimumWidth(0);
    m_meta->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    metaBox->addWidget(m_meta);
    auto *hint = new QLabel(QStringLiteral("Runs through Proton with the fix's DLL overrides applied."));
    hint->setObjectName(QStringLiteral("small"));
    hint->setWordWrap(true);
    hint->setMinimumWidth(0);
    hint->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    metaBox->addWidget(hint);
    metaBox->addStretch();
    topRow->addLayout(metaBox, 1);

    m_playButton = new QPushButton(QStringLiteral("Play"));
    m_playButton->setObjectName(QStringLiteral("primary"));
    m_playButton->setIcon(Icons::icon(Icons::Name::Play, QColor(0x1a, 0x12, 0x06), 18));
    m_playButton->setIconSize(QSize(18, 18));
    m_playButton->setMinimumWidth(140);
    m_playButton->setMinimumHeight(42);
    connect(m_playButton, &QPushButton::clicked, this, [this] {
        if (m_running)
            emit stopRequested();
        else
            emit playRequested(m_game.name);
    });
    topRow->addWidget(m_playButton, 0, Qt::AlignVCenter);

    m_settingsButton = new QPushButton(QStringLiteral("Settings"));
    m_settingsButton->setIcon(Icons::icon(Icons::Name::Settings, Theme::TextMuted, 17));
    m_settingsButton->setIconSize(QSize(17, 17));
    m_settingsButton->setMinimumHeight(42);
    connect(m_settingsButton, &QPushButton::clicked, this, [this] {
        emit settingsRequested(m_game.name);
    });
    topRow->addWidget(m_settingsButton, 0, Qt::AlignVCenter);

    layout->addLayout(topRow);

    // Secondary actions
    auto *actionHost = new QWidget;
    auto *actions = new FlowLayout(actionHost, 0, 4, 4);

    auto *back = makeActionButton(QStringLiteral("Library"), Icons::Name::Back);
    connect(back, &QPushButton::clicked, this, &GameDetailPage::backRequested);
    actions->addWidget(back);

    auto *separator = new QFrame;
    separator->setFrameShape(QFrame::VLine);
    separator->setStyleSheet(QStringLiteral("color: #2e2e3a;"));
    separator->setFixedHeight(22);
    actions->addWidget(separator);

    m_debugButton = makeActionButton(QStringLiteral("Debug"), Icons::Name::Debug);
    connect(m_debugButton, &QPushButton::clicked, this, [this] {
        emit debugRequested(m_game.name);
    });
    actions->addWidget(m_debugButton);

    m_utilityButton = new QToolButton;
    m_utilityButton->setText(QStringLiteral("Wine tools"));
    m_utilityButton->setIcon(Icons::icon(Icons::Name::Wine, Theme::TextMuted, 17));
    m_utilityButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_utilityButton->setPopupMode(QToolButton::InstantPopup);
    m_utilityButton->setCursor(Qt::PointingHandCursor);
    m_utilityButton->setStyleSheet(QStringLiteral(
        "QToolButton { background: transparent; border: none; border-radius: 8px; padding: 6px 10px;"
        " color: #c9c9d4; } QToolButton:hover { background: #24242d; }"));
    auto *utilityMenu = new QMenu(m_utilityButton);
    utilityMenu->addAction(QStringLiteral("Wine configuration"), this, [this] {
        emit utilityRequested(m_game.name, QStringLiteral("winecfg.exe"));
    });
    utilityMenu->addAction(QStringLiteral("Task manager"), this, [this] {
        emit utilityRequested(m_game.name, QStringLiteral("taskmgr.exe"));
    });
    utilityMenu->addAction(QStringLiteral("Control panel"), this, [this] {
        emit utilityRequested(m_game.name, QStringLiteral("control.exe"));
    });
    utilityMenu->addAction(QStringLiteral("Explorer"), this, [this] {
        emit utilityRequested(m_game.name, QStringLiteral("explorer.exe"));
    });
    utilityMenu->addAction(QStringLiteral("Command prompt"), this, [this] {
        emit utilityRequested(m_game.name, QStringLiteral("wineconsole"));
    });
    utilityMenu->addAction(QStringLiteral("Registry editor"), this, [this] {
        emit utilityRequested(m_game.name, QStringLiteral("regedit.exe"));
    });
    utilityMenu->addSeparator();
    QAction *winetricks = utilityMenu->addAction(QStringLiteral("Winetricks"), this, [this] {
        emit utilityRequested(m_game.name, QStringLiteral("winetricks"));
    });
    if (!Utils::which(QStringLiteral("winetricks")))
        winetricks->setEnabled(false);
    m_utilityButton->setMenu(utilityMenu);
    actions->addWidget(m_utilityButton);

    m_folderButton = new QToolButton;
    m_folderButton->setText(QStringLiteral("Folders"));
    m_folderButton->setIcon(Icons::icon(Icons::Name::Folder, Theme::TextMuted, 17));
    m_folderButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_folderButton->setPopupMode(QToolButton::InstantPopup);
    m_folderButton->setCursor(Qt::PointingHandCursor);
    m_folderButton->setStyleSheet(m_utilityButton->styleSheet());
    auto *folderMenu = new QMenu(m_folderButton);
    folderMenu->addAction(QStringLiteral("Open game folder"), this, [this] {
        QString path = m_game.mainPath;
        if (path.isEmpty())
            path = QFileInfo(m_game.executable).absolutePath();
        emit openFolderRequested(path);
    });
    folderMenu->addAction(QStringLiteral("Open prefix folder"), this, [this] {
        emit openFolderRequested(m_game.winePrefixPath());
    });
    m_folderButton->setMenu(folderMenu);
    actions->addWidget(m_folderButton);

    m_runButton = makeActionButton(QStringLiteral("Run .exe"), Icons::Name::Run);
    connect(m_runButton, &QPushButton::clicked, this, [this] {
        emit runExeRequested(m_game.name);
    });
    actions->addWidget(m_runButton);

    m_logButton = makeActionButton(QStringLiteral("Log"), Icons::Name::Terminal);
    connect(m_logButton, &QPushButton::clicked, this, [this] {
        emit logRequested(m_game.name);
    });
    actions->addWidget(m_logButton);

    m_steamButton = makeActionButton(QStringLiteral("Steam"), Icons::Name::External);
    m_protonDbButton = makeActionButton(QStringLiteral("ProtonDB"), Icons::Name::External);
    m_steamDbButton = makeActionButton(QStringLiteral("SteamDB"), Icons::Name::External);
    connect(m_steamButton, &QPushButton::clicked, this, [this] {
        emit linkRequested(QStringLiteral("https://store.steampowered.com/app/") + m_game.steamId);
    });
    connect(m_protonDbButton, &QPushButton::clicked, this, [this] {
        emit linkRequested(QStringLiteral("https://www.protondb.com/app/") + m_game.steamId);
    });
    connect(m_steamDbButton, &QPushButton::clicked, this, [this] {
        emit linkRequested(QStringLiteral("https://steamdb.info/app/") + m_game.steamId);
    });
    actions->addWidget(m_steamButton);
    actions->addWidget(m_protonDbButton);
    actions->addWidget(m_steamDbButton);

    m_removeButton = makeActionButton(QStringLiteral("Remove"), Icons::Name::Trash);
    m_removeButton->setObjectName(QStringLiteral("danger"));
    connect(m_removeButton, &QPushButton::clicked, this, [this] {
        emit removeRequested(m_game.name);
    });
    actions->addWidget(m_removeButton);

    layout->addWidget(actionHost);

    // Info cards
    m_infoHost = new QWidget;
    auto *infoLayout = new QGridLayout(m_infoHost);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setHorizontalSpacing(16);
    infoLayout->setVerticalSpacing(16);
    infoLayout->setColumnStretch(0, 1);
    infoLayout->setColumnStretch(1, 1);
    layout->addWidget(m_infoHost);

    // Shortcuts
    auto *shortcutCard = new QFrame;
    shortcutCard->setObjectName(QStringLiteral("surface"));
    auto *shortcutLayout = new QHBoxLayout(shortcutCard);
    shortcutLayout->setContentsMargins(18, 14, 18, 14);
    shortcutLayout->setSpacing(24);
    auto *shortcutTitle = new QLabel(QStringLiteral("Shortcuts"));
    shortcutTitle->setObjectName(QStringLiteral("sectionTitle"));
    shortcutLayout->addWidget(shortcutTitle);
    m_desktopShortcut = new QCheckBox(QStringLiteral("Desktop"));
    m_menuShortcut = new QCheckBox(QStringLiteral("Application menu"));
    shortcutLayout->addWidget(m_desktopShortcut);
    shortcutLayout->addWidget(m_menuShortcut);
    shortcutLayout->addStretch();
    connect(m_desktopShortcut, &QCheckBox::toggled, this, [this](bool checked) {
        emit shortcutToggled(m_game.name, true, checked);
    });
    connect(m_menuShortcut, &QCheckBox::toggled, this, [this](bool checked) {
        emit shortcutToggled(m_game.name, false, checked);
    });
    layout->addWidget(shortcutCard);
    layout->addStretch();

    scroll->setWidget(content);
    outer->addWidget(scroll);
}

void GameDetailPage::setGame(const Game &game)
{
    m_game = game;
    m_playTime = game.timeSpent;
    m_banner->setImagePath(game.banner);
    m_banner->setTitle(game.name);
    applyIcon();
    rebuildInfo();

    m_steamButton->setVisible(!game.steamId.isEmpty());
    m_protonDbButton->setVisible(!game.steamId.isEmpty());
    m_steamDbButton->setVisible(!game.steamId.isEmpty());
    updatePlayButton();
}

void GameDetailPage::setPlayTime(qint64 seconds)
{
    m_playTime = seconds;
    m_banner->setSubtitle(QStringLiteral("%1 played  •  %2")
                              .arg(Utils::humanDuration(m_playTime),
                                   m_game.proton.isEmpty() ? QStringLiteral("Proton") : m_game.proton));
    rebuildInfo();
}

void GameDetailPage::setRunning(bool running)
{
    m_running = running;
    updatePlayButton();
}

void GameDetailPage::setLaunchBusy(bool busy)
{
    m_busy = busy;
    if (busy)
        m_stopping = false;
    updatePlayButton();
}

void GameDetailPage::setStopping(bool stopping)
{
    m_stopping = stopping;
    updatePlayButton();
}

void GameDetailPage::setShortcuts(bool desktop, bool menu)
{
    QSignalBlocker blockDesktop(m_desktopShortcut);
    QSignalBlocker blockMenu(m_menuShortcut);
    m_desktopShortcut->setChecked(desktop);
    m_menuShortcut->setChecked(menu);
}

void GameDetailPage::applyIcon()
{
    QPixmap source;
    if (!m_game.icon.isEmpty())
        source.load(m_game.icon);
    if (source.isNull()) {
        m_icon->setPixmap(Icons::pixmap(Icons::Name::Game, Theme::TextMuted, 34));
        m_icon->setAlignment(Qt::AlignCenter);
        return;
    }
    m_icon->setPixmap(roundedPixmap(source, 64, 14));
    m_icon->setAlignment(Qt::AlignCenter);
}

void GameDetailPage::updatePlayButton()
{
    m_playButton->setEnabled(!m_busy && !m_stopping);
    if (m_stopping) {
        m_playButton->setText(QStringLiteral("Closing…"));
        m_playButton->setIcon(Icons::icon(Icons::Name::Stop, QColor(0x1a, 0x12, 0x06), 18));
        m_playButton->setObjectName(QStringLiteral("primary"));
    } else if (m_busy) {
        m_playButton->setText(QStringLiteral("Starting…"));
        m_playButton->setIcon(Icons::icon(Icons::Name::Clock, QColor(0x1a, 0x12, 0x06), 18));
        m_playButton->setObjectName(QStringLiteral("primary"));
    } else if (m_running) {
        m_playButton->setText(QStringLiteral("Close game"));
        m_playButton->setIcon(Icons::icon(Icons::Name::Stop, QColor(0x1a, 0x12, 0x06), 18));
        m_playButton->setObjectName(QStringLiteral("primary"));
    } else {
        m_playButton->setText(QStringLiteral("Play"));
        m_playButton->setIcon(Icons::icon(Icons::Name::Play, QColor(0x1a, 0x12, 0x06), 18));
        m_playButton->setObjectName(QStringLiteral("primary"));
    }
    m_playButton->style()->unpolish(m_playButton);
    m_playButton->style()->polish(m_playButton);
}

void GameDetailPage::rebuildInfo()
{
    m_banner->setSubtitle(QStringLiteral("%1 played  •  %2")
                              .arg(Utils::humanDuration(m_playTime),
                                   m_game.proton.isEmpty() ? QStringLiteral("Proton") : m_game.proton));
    m_meta->setText(QStringLiteral("Played %1  •  %2  •  %3")
                        .arg(Utils::humanDuration(m_playTime),
                             m_game.proton.isEmpty() ? QStringLiteral("Proton") : m_game.proton,
                             m_game.steamOverlay ? QStringLiteral("Steam overlay on")
                                                 : QStringLiteral("Steam overlay off")));

    auto *grid = qobject_cast<QGridLayout *>(m_infoHost->layout());
    while (QLayoutItem *item = grid->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    QList<QPair<QString, QString>> launchRows;
    launchRows.append({QStringLiteral("Proton"), m_game.proton.isEmpty()
                                                    ? QStringLiteral("GE-Proton Latest")
                                                    : m_game.proton});
    launchRows.append({QStringLiteral("Steam Runtime"),
                       m_game.steamRuntime ? QStringLiteral("Enabled") : QStringLiteral("Disabled")});
    launchRows.append({QStringLiteral("Native Wayland"),
                       m_game.nativeWayland ? QStringLiteral("Enabled") : QStringLiteral("Disabled")});
    launchRows.append({QStringLiteral("WINED3D"),
                       m_game.wined3d ? QStringLiteral("Enabled") : QStringLiteral("DXVK (Proton default)")});
    launchRows.append({QStringLiteral("Arguments"),
                       QStringList{m_game.argsBefore, m_game.argsAfter}.join(QLatin1Char(' ')).trimmed()});

    QList<QPair<QString, QString>> gameRows;
    gameRows.append({QStringLiteral("Executable"), m_game.executable});
    gameRows.append({QStringLiteral("Prefix"), m_game.effectivePrefixPath()});
    if (!m_game.steamId.isEmpty())
        gameRows.append({QStringLiteral("Steam AppID"), m_game.steamId});
    if (!m_game.fakeSteamId.isEmpty())
        gameRows.append({QStringLiteral("Overlay AppID"), m_game.fakeSteamId});
    gameRows.append({QStringLiteral("DLL overrides"), m_game.overrides});

    grid->addWidget(makeInfoCard(QStringLiteral("Launch"), launchRows), 0, 0);
    grid->addWidget(makeInfoCard(QStringLiteral("Game files"), gameRows), 0, 1);
}
