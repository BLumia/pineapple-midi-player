// SPDX-FileCopyrightText: 2024 Gary Wang <git@blumia.net>
//
// SPDX-License-Identifier: MIT

#include "settings.h"

#include <QApplication>
#include <QStandardPaths>
#include <QDebug>
#include <QDir>
#include <QMetaEnum>

namespace QEnumHelper
{
    template <typename E>
    E fromString(const QString &text, const E defaultValue)
    {
        bool ok;
        E result = static_cast<E>(QMetaEnum::fromType<E>().keyToValue(text.toUtf8(), &ok));
        if (!ok) {
            return defaultValue;
        }
        return result;
    }

    template <typename E>
    QString toString(E value)
    {
        const int intValue = static_cast<int>(value);
        return QString::fromUtf8(QMetaEnum::fromType<E>().valueToKey(intValue));
    }
}

Settings *Settings::m_settings_instance = nullptr;

Settings *Settings::instance()
{
    if (!m_settings_instance) {
        m_settings_instance = new Settings;
    }

    return m_settings_instance;
}

bool Settings::stayOnTop() const
{
    return m_qsettings->value("stay_on_top", true).toBool();
}

void Settings::setStayOnTop(bool on)
{
    m_qsettings->setValue("stay_on_top", on);
    m_qsettings->sync();
}

QString Settings::applicationStyle() const
{
    return m_qsettings->value("application_style", QString()).toString();
}

QString Settings::fallbackSoundFont() const
{
    return m_qsettings->value("fallback_soundfont", QString()).toString();
}

void Settings::setApplicationStyle(const QString &styleName)
{
    m_qsettings->setValue("application_style", styleName);
    m_qsettings->sync();
}

void Settings::setFallbackSoundFont(const QString &path)
{
    m_qsettings->setValue("fallback_soundfont", path);
    m_qsettings->sync();
}

#if defined(FLAG_PORTABLE_MODE_SUPPORT) && defined(Q_OS_WIN)
#include <windows.h>
// QCoreApplication::applicationDirPath() parses the "applicationDirPath" from arg0, which...
// 1. rely on a QApplication object instance
//    but we need to call QGuiApplication::setHighDpiScaleFactorRoundingPolicy() before QApplication get created
// 2. arg0 is NOT garanteed to be the path of execution
//    see also: https://stackoverflow.com/questions/383973/is-args0-guaranteed-to-be-the-path-of-execution
// This function is here mainly for #1.
QString getApplicationDirPath()
{
    WCHAR buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    QString appPath = QString::fromWCharArray(buffer);

    return appPath.left(appPath.lastIndexOf('\\'));
}
#endif // defined(FLAG_PORTABLE_MODE_SUPPORT) && defined(Q_OS_WIN)

Settings::Settings()
    : QObject(qApp)
{
    QString configPath;

#if defined(FLAG_PORTABLE_MODE_SUPPORT) && defined(Q_OS_WIN)
    QString portableConfigDirPath = QDir(getApplicationDirPath()).absoluteFilePath("data");
    QFileInfo portableConfigDirInfo(portableConfigDirPath);
    if (portableConfigDirInfo.exists() && portableConfigDirInfo.isDir() && portableConfigDirInfo.isWritable()) {
        // we can use it.
        configPath = portableConfigDirPath;
    }
#endif // defined(FLAG_PORTABLE_MODE_SUPPORT) && defined(Q_OS_WIN)

    if (configPath.isEmpty()) {
        // %LOCALAPPDATA%\<APPNAME> under Windows, ~/.config/<APPNAME> under Linux.
        configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    }

    m_qsettings = new QSettings(QDir(configPath).absoluteFilePath("config.ini"), QSettings::IniFormat, this);
}

