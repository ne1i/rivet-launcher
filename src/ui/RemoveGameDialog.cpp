#include "RemoveGameDialog.h"

#include "DesktopEntry.h"
#include "Icons.h"
#include "Paths.h"
#include "Theme.h"
#include "Utils.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

RemoveGameDialog::RemoveGameDialog(Config *store, const Game &game, QWidget *parent)
    : QDialog(parent)
    , m_store(store)
    , m_game(game)
{
    setWindowTitle(QStringLiteral("Remove %1").arg(game.name));
    setMinimumWidth(600);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 18);
    layout->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("Remove %1?").arg(game.name));
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);

    auto *hint = new QLabel(QStringLiteral(
        "The game is removed from the launcher, along with its shortcuts. "
        "Nothing else is deleted unless you select it below."));
    hint->setObjectName(QStringLiteral("muted"));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    const QString prefix = game.effectivePrefixPath();
    const QString mainPath = game.mainPath.isEmpty()
        ? QFileInfo(game.executable).absolutePath()
        : game.mainPath;

    auto *prefixLabel = new QLabel;
    prefixLabel->setObjectName(QStringLiteral("small"));
    prefixLabel->setWordWrap(true);
    prefixLabel->setText(QStringLiteral("Prefix: %1").arg(prefix));
    auto *filesLabel = new QLabel;
    filesLabel->setObjectName(QStringLiteral("small"));
    filesLabel->setWordWrap(true);
    filesLabel->setText(QStringLiteral("Game files: %1").arg(mainPath));

    m_removePrefix = new QCheckBox(QStringLiteral("Delete the game prefix"));
    const bool prefixExists = QFileInfo(prefix).isDir();
    m_removePrefix->setEnabled(prefixExists);
    m_removePrefix->setChecked(prefixExists);
    if (!prefixExists)
        m_removePrefix->setText(QStringLiteral("Delete the game prefix (not created yet)"));

    m_removeFiles = new QCheckBox(QStringLiteral("Delete the game files from disk"));
    const bool filesExist = QFileInfo(mainPath).isDir();
    m_removeFiles->setEnabled(filesExist);
    m_removeFiles->setChecked(false);
    if (!filesExist)
        m_removeFiles->setText(QStringLiteral("Delete the game files from disk (folder not found)"));

    layout->addWidget(m_removePrefix);
    layout->addWidget(prefixLabel);
    layout->addWidget(m_removeFiles);
    layout->addWidget(filesLabel);

    auto *warning = new QLabel(QStringLiteral(
        "This cannot be undone. Save data and mods inside the game folder will be deleted too."));
    warning->setObjectName(QStringLiteral("error"));
    warning->setWordWrap(true);
    layout->addWidget(warning);

    m_error = new QLabel;
    m_error->setObjectName(QStringLiteral("error"));
    m_error->setWordWrap(true);
    layout->addWidget(m_error);
    layout->addStretch();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    auto *removeButton = new QPushButton(QStringLiteral("Remove game"));
    removeButton->setObjectName(QStringLiteral("danger"));
    removeButton->setIcon(Icons::icon(Icons::Name::Trash, Theme::Danger, 16));
    buttons->addButton(removeButton, QDialogButtonBox::AcceptRole);
    connect(removeButton, &QPushButton::clicked, this, &RemoveGameDialog::remove);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void RemoveGameDialog::remove()
{
    const auto answer = QMessageBox::warning(
        this, QStringLiteral("Remove %1").arg(m_game.name),
        QStringLiteral("Remove this game now?"), QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);

    const QString prefix = m_game.effectivePrefixPath();
    const QString mainPath = m_game.mainPath.isEmpty()
        ? QFileInfo(m_game.executable).absolutePath()
        : m_game.mainPath;

    QStringList errors;
    if (m_removePrefix->isChecked() && QFileInfo(prefix).isDir()) {
        QString error;
        if (!Utils::removeRecursively(prefix, &error))
            errors.append(error);
    }

    if (m_removeFiles->isChecked() && QFileInfo(mainPath).isDir()) {
        if (mainPath == QDir::homePath() || mainPath == QLatin1String("/")) {
            errors.append(QStringLiteral("Refusing to delete a system folder: %1").arg(mainPath));
        } else {
            QString error;
            if (!Utils::removeRecursively(mainPath, &error))
                errors.append(error);
        }
    }

    const QString desktop = Paths::desktopDir();
    const QString menu = Paths::applicationsDir();
    DesktopEntry::remove(desktop, m_game.name);
    DesktopEntry::remove(menu, m_game.name);

    const auto usedElsewhere = [this](const QString &path) {
        if (path.isEmpty())
            return false;
        for (const QString &other : m_store->gameNames()) {
            if (other == m_game.name)
                continue;
            const Game otherGame = m_store->game(other);
            if (otherGame.icon == path || otherGame.banner == path)
                return true;
        }
        return false;
    };

    if (!m_game.icon.isEmpty() && QFileInfo(m_game.icon).isFile()
        && !usedElsewhere(m_game.icon))
        QFile::remove(m_game.icon);
    if (!m_game.banner.isEmpty() && QFileInfo(m_game.banner).isFile()
        && !usedElsewhere(m_game.banner))
        QFile::remove(m_game.banner);

    QString storeError;
    if (!m_store->removeGame(m_game.name, &storeError))
        errors.append(storeError);

    QApplication::restoreOverrideCursor();

    if (!errors.isEmpty()) {
        m_error->setText(errors.join(QLatin1Char('\n')));
        QMessageBox::warning(this, QStringLiteral("Some files were not removed"),
                             errors.join(QLatin1Char('\n')));
        return;
    }

    emit removed(m_game.name);
    accept();
}
