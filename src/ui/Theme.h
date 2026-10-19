#pragma once

#include <QColor>
#include <QString>

class QApplication;

namespace Theme {

inline const QColor Background{0x10, 0x10, 0x14};
inline const QColor BackgroundElevated{0x16, 0x16, 0x1c};
inline const QColor Surface{0x1d, 0x1d, 0x25};
inline const QColor SurfaceHover{0x26, 0x26, 0x31};
inline const QColor Border{0x2e, 0x2e, 0x3a};
inline const QColor Text{0xf4, 0xf4, 0xf7};
inline const QColor TextMuted{0x9a, 0x9a, 0xab};
inline const QColor Accent{0xf2, 0xa9, 0x3b};
inline const QColor AccentHover{0xff, 0xbe, 0x55};
inline const QColor Danger{0xe5, 0x48, 0x4d};
inline const QColor Success{0x46, 0xc0, 0x7a};

void apply(QApplication &application);
QString styleSheet();

} // namespace Theme
