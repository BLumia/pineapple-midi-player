// SPDX-FileCopyrightText: 2024 Gary Wang <git@blumia.net>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QObject>
#include <QSettings>

class Settings : public QObject
{
    Q_OBJECT
public:
    static Settings *instance();

    bool stayOnTop() const;
    QString applicationStyle() const;
    QString fallbackSoundFont() const;

    void setStayOnTop(bool on);
    void setApplicationStyle(const QString & styleName);
    void setFallbackSoundFont(const QString & path);

private:
    Settings();

    static Settings *m_settings_instance;

    QSettings *m_qsettings;

signals:

public slots:
};

