#pragma once

#include "Config.h"

#include <QDialog>

class QCheckBox;
class QLabel;

class RemoveGameDialog : public QDialog
{
    Q_OBJECT
public:
    RemoveGameDialog(Config *store, const Game &game, QWidget *parent = nullptr);

signals:
    void removed(const QString &gameName);

private:
    void remove();

    Config *m_store;
    Game m_game;
    QCheckBox *m_removePrefix = nullptr;
    QCheckBox *m_removeFiles = nullptr;
    QLabel *m_error = nullptr;
};
