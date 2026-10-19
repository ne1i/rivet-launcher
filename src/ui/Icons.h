#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace Icons {

enum class Name {
    Play,
    Stop,
    Plus,
    Settings,
    Trash,
    Folder,
    Debug,
    Search,
    Back,
    Download,
    Save,
    Edit,
    Wine,
    Link,
    External,
    Clock,
    Terminal,
    Run,
    Gamepad,
    Close,
    Refresh,
    Monitor,
    Check,
    Warning,
    Info,
    More,
    Copy,
    Game,
};

QIcon icon(Name name, const QColor &color = QColor(0xf4, 0xf4, 0xf7), int size = 20);
QPixmap pixmap(Name name, const QColor &color = QColor(0xf4, 0xf4, 0xf7), int size = 20);
QIcon appIcon();

} // namespace Icons
