// SPDX-FileCopyrightText: 2024 Gary Wang <git@blumia.net>
//
// SPDX-License-Identifier: MIT

#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QUrl>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // parse commandline arguments
    QCommandLineParser parser;
    parser.addPositionalArgument("File list", QCoreApplication::translate("main", "File list."));
    parser.addHelpOption();
    parser.process(a);

    MainWindow w;
    w.show();

    QStringList urlStrList = parser.positionalArguments();
    QList<QUrl> urlList = MainWindow::convertToUrlList(urlStrList);
    if (!urlList.isEmpty()) {
        w.tryLoadFiles(urlList);
    }

    return a.exec();
}
