// SPDX-FileCopyrightText: 2025 Gary Wang <git@blumia.net>
//
// SPDX-License-Identifier: MIT

#include "mainwindow.h"
#include "settings.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QTranslator>
#include <QUrl>

#ifdef CONAN2_STATIC_QT_BUG
#include <QtPlugin>
Q_IMPORT_PLUGIN (QWindowsIntegrationPlugin)
#endif // CONAN2_STATIC_QT_BUG

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTranslator translator;
    if (translator.load(QLocale(), QLatin1String("pineapple-midi-player"), QLatin1String("."), QLatin1String(":/i18n"))) {
        QApplication::installTranslator(&translator);
    }
    QApplication::setApplicationName("Pineapple MIDI Player");
    QApplication::setApplicationDisplayName(QCoreApplication::translate("main", "Pineapple MIDI Player"));

    // parse commandline arguments
    QCommandLineParser parser;
    parser.addPositionalArgument("File list", QCoreApplication::translate("main", "File list."));
    parser.addHelpOption();
    parser.process(a);

    if (!Settings::instance()->applicationStyle().isEmpty()) {
        qApp->setStyle(Settings::instance()->applicationStyle());
    }

    MainWindow w;
    w.show();

    QStringList urlStrList = parser.positionalArguments();
    QList<QUrl> urlList = MainWindow::convertToUrlList(urlStrList);
    if (!urlList.isEmpty()) {
        w.tryLoadFiles(urlList);
    }

    return QApplication::exec();
}
