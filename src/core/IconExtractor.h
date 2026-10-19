#pragma once

#include <QString>

class IconExtractor
{
public:
    static QString extractFromExecutable(const QString &executable, QString *error = nullptr);
    static QString copyImage(const QString &imagePath, QString *error = nullptr);
};
