// SPDX-FileCopyrightText: 2025 Gary Wang <git@blumia.net>
//
// SPDX-License-Identifier: MIT

#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "player.h"
#include "settings.h"
#include "playlistmanager.h"

#ifdef Q_OS_WIN
// copied from KIO, thus we won't depend on KIO for the open with dialog
// note that KIO on Linux also offer extra menu actions than just a open with dialog,
// so it's still suggested to use KIO under Linux
#include "widgetsopenwithhandler_win.cpp" // displayNativeOpenWithDialog
#endif

#include <QDir>
#include <QDropEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QMessageBox>
#include <QFileDialog>
#include <QPushButton>
#include <QStringList>
#include <QStyleFactory>
#include <QStandardPaths>
#include <QStringBuilder>
#include <QTimer>

#ifdef HAVE_KIO
#include <KFileItemActions>
#include <KFileItemListProperties>
#endif // HAVE_KIO

#include <portaudio.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_detectedSoundfontsMenu(nullptr)
    , m_playlistManager(new PlaylistManager(this))
{
    ui->setupUi(this);
    setAcceptDrops(true);
    ui->playlistView->setModel(m_playlistManager->model());
    ui->playlistView->setVisible(ui->actionTogglePlaylist->isChecked());
    ui->togglePlaylistBtn->setDefaultAction(ui->actionTogglePlaylist);

    m_playlistManager->setAutoLoadFilterSuffixes({"*.mid"});

    generateThemeMenu();
    adjustSize();

    setWindowFlag(Qt::WindowStaysOnTopHint, Settings::instance()->stayOnTop());
    ui->actionStayOnTop->setChecked(Settings::instance()->stayOnTop());

    scanSoundfonts();
    updateFallbackSoundFontAction(Settings::instance()->fallbackSoundFont());

    connect(ui->actionFallbackSoundFont, &QAction::triggered, this, &MainWindow::loadSpecificSoundFontActionTriggered);

    m_playbackUiTimer = new QTimer(this);
    m_playbackUiTimer->setInterval(33);
    connect(m_playbackUiTimer, &QTimer::timeout, this, [this]() {
        playerPlaybackCallback(Player::instance()->currentPlaybackPositionMs());
    });

    Player::instance()->onIsPlayingChanged([this](bool isPlaying){
        ui->playBtn->setText(isPlaying ? QCoreApplication::translate("MainWindow", "Pause")
                                       : QCoreApplication::translate("MainWindow", "Play"));
        ui->playBtn->setIcon(isPlaying ? QIcon::fromTheme("media-playback-pause")
                                       : QIcon::fromTheme("media-playback-start"));
        if (m_playbackUiTimer) {
            isPlaying ? m_playbackUiTimer->start() : m_playbackUiTimer->stop();
        }
    });

    connect(this, &MainWindow::midiFileLoaded, this, [this](const QString path){
        QFileInfo fi(path);
        ui->midiFileLabel->setText(fi.fileName());
        ui->midiFileLabel->setToolTip(fi.fileName());
        m_currentMidiFilePath = path;

        auto info = Player::instance()->midiInfo();
        unsigned int duration = std::get<unsigned int>(info[Player::I_LENGTH_MS]);
        ui->seekSlider->setMaximum(duration);
        ui->durationLabel->setText(QTime::fromMSecsSinceStartOfDay(0).addMSecs(duration).toString("m:ss").toLatin1().data());

        // menu state
        ui->actionConvertToWav->setEnabled(true);
        ui->actionOpenWith->setEnabled(true);
#ifdef HAVE_KIO
        QMenu * openWithSubmenu = new QMenu();
        KFileItemActions * actions = new KFileItemActions(openWithSubmenu);
        KFileItemListProperties itemProperties(KFileItemList{KFileItem(QUrl::fromLocalFile(path))});
        actions->setItemListProperties(itemProperties);
        actions->setParentWidget(openWithSubmenu);
        actions->insertOpenWithActionsTo(nullptr, openWithSubmenu, QStringList());
        ui->actionOpenWith->setMenu(openWithSubmenu);
#endif // HAVE_KIO
    });

    connect(this, &MainWindow::soundfontLoaded, this, [this](const QString& path){
        const QFileInfo fi(path);
        ui->soundFontFileLabel->setText(fi.fileName());
        m_currentSf2FilePath = path;
    });

    connect(m_playlistManager, &PlaylistManager::currentIndexChanged, this, [this](int index){
        ui->playlistView->setCurrentIndex(m_playlistManager->curIndex());
    });
}

MainWindow::~MainWindow()
{
    delete Player::instance(); // the callback might call UI thread, so delete it first
    delete ui;
}

bool MainWindow::loadMidiFile(const QString &path)
{
    QFileInfo fi(path);
    if (fi.exists()) {
        bool succ = Player::instance()->loadMidiFile(QFile::encodeName(path));
        if (succ) {
            emit midiFileLoaded(path);
            // load same folder soundfont if exists
            const QString sameFolderSoundFont(fi.dir().absoluteFilePath(fi.baseName() + ".sf2"));
            if (QFileInfo::exists(sameFolderSoundFont)) {
                bool sf2succ = Player::instance()->loadSF2File(QFile::encodeName(sameFolderSoundFont));
                if (sf2succ) {
                    emit soundfontLoaded(sameFolderSoundFont);
                }
            }
        }
        return succ;
    }
    return false;
}

void MainWindow::loadSoundFontFile(const QString &path)
{
    if (QFileInfo::exists(path)) {
        bool succ = Player::instance()->loadSF2File(QFile::encodeName(path));
        if (succ) {
            emit soundfontLoaded(path);
        }
    }
}

void MainWindow::loadOP2File(const QString &path)
{
    if (QFileInfo::exists(path)) {
        bool succ = Player::instance()->loadOP2File(QFile::encodeName(path));
        qDebug() << "op2 file:" << path << succ;
    }
}

void MainWindow::tryLoadFiles(const QList<QUrl> &urls, bool tryPlayAfterLoad, bool suppressWarningDlg)
{
    const QStringList soundfontFormats{"sf2", "sf3"};
    QList<QUrl> loadedMidiFiles;

    for (const QUrl & url : urls) {
        QFileInfo fi(url.toLocalFile());
        QString suffix(fi.suffix().toLower());
        if (suffix == "mid") {
            bool succ = loadMidiFile(fi.absoluteFilePath());
            if (succ) {
                loadedMidiFiles << QUrl::fromLocalFile(fi.absoluteFilePath());
            }
        } else if (soundfontFormats.contains(fi.suffix().toLower())) {
            loadSoundFontFile(fi.absoluteFilePath());
        } else if (suffix == "op2") {
            loadOP2File(fi.absoluteFilePath());
        }
    }

    m_playlistManager->loadPlaylist(loadedMidiFiles);

    if (tryPlayAfterLoad) {
        tryPlay(suppressWarningDlg);
    }
}

bool MainWindow::checkCanPlay(bool suppressWarningDlg)
{
    if (m_currentMidiFilePath.isEmpty()) {
        if (!suppressWarningDlg) QMessageBox::information(this, tr("Missing MIDI file"), tr("Please load a MIDI file first."));
        return false;
    }

    if (!ensureWeHaveSoundfont()) {
        if (!suppressWarningDlg) {
            m_noSoundFontInfoBoxShownTimesCount++;

            QMessageBox infoBox(this);

            QString helpText = tr("You need to select a SoundFont before play the MIDI file.");
            if (m_noSoundFontInfoBoxShownTimesCount >= 3) {
                helpText += QString("\n\n") + tr("You have seen this dialog for %1 times, if you really don't want to use a SoundFont, you can now ignore this warning if you want.").arg(m_noSoundFontInfoBoxShownTimesCount);
                infoBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Ignore);
            }

            infoBox.setIcon(QMessageBox::Information);
            infoBox.setWindowTitle(tr("Missing SoundFont"));
            infoBox.setInformativeText(tr("If you don't know where to get a SoundFont, check out <a href='https://musescore.org/en/handbook/3/soundfonts-and-sfz-files#list'>this page</a> provided by MuseScore."));
            infoBox.setText(helpText);
            infoBox.setTextFormat(Qt::MarkdownText);
            infoBox.exec();
            if (infoBox.result() == QMessageBox::Ignore) {
                // easter egg: will use the opl engine to do the playback.
                return true;
            }
        }
        return false;
    }

    return true;
}

void MainWindow::tryPlay(bool suppressWarningDlg)
{
    if (checkCanPlay(suppressWarningDlg)) {
        Player::instance()->play();
    }
}

QList<QUrl> MainWindow::convertToUrlList(const QStringList &files)
{
    QList<QUrl> urlList;
    for (const QString & str : std::as_const(files)) {
        QUrl url = QUrl::fromLocalFile(str);
        if (url.isValid()) {
            urlList.append(url);
        }
    }

    return urlList;
}

void MainWindow::loadSpecificSoundFontActionTriggered()
{
    QAction * action = qobject_cast<QAction*>(QObject::sender());
    loadSoundFontFile(action->data().toString());
    if (QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier)) {
        Settings::instance()->setFallbackSoundFont(action->data().toString());
    }
}

void MainWindow::on_playBtn_clicked()
{
    Player::instance()->isPlaying() ? Player::instance()->pause() : tryPlay(false);
}

void MainWindow::on_stopBtn_clicked()
{
    Player::instance()->stop();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }

    return QMainWindow::dragEnterEvent(event);
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
    Q_UNUSED(event)
}

void MainWindow::dropEvent(QDropEvent *event)
{
    event->acceptProposedAction();

    const QMimeData * mimeData = event->mimeData();

    if (mimeData->hasUrls()) {
        const QList<QUrl> &urls = mimeData->urls();
        tryLoadFiles(urls, ui->actionPlayOnDrop->isChecked(), true);
    }
}

// return false if we don't have any soundfont.
bool MainWindow::ensureWeHaveSoundfont()
{
    if (m_currentSf2FilePath.isEmpty()) {
        if (ui->actionFallbackSoundFont->isEnabled()) {
            ui->actionFallbackSoundFont->trigger();
            return true;
        } else {
            // TODO: maybe select one from the detected soundfonts?
            return false;
        }
    } else {
        return true;
    }
}

bool MainWindow::scanSoundfonts()
{
    QFileInfoList results;
    QStringList paths(QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation));
    paths.append(QCoreApplication::applicationDirPath());
    paths.removeDuplicates(); // under Windows, the app dir is already a part of GenericDataLocation
    for (const QString & path : std::as_const(paths)) {
        const QDir dir(path);
        if (!dir.exists()) continue;
        // Arch flavor soundfonts folder, may contain sf2 and sf3 file.
        QDir soundfontsSubfolder(dir.filePath("soundfonts"));
        if (soundfontsSubfolder.exists()) {
            qDebug() << soundfontsSubfolder.absolutePath();
            soundfontsSubfolder.setNameFilters(QStringList{"*.sf2", "*.sf3"});
            soundfontsSubfolder.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);
            results.append(soundfontsSubfolder.entryInfoList());
        }
        // Debian flavor soundfonts folder, we only scan sf2 here since tsf's sf3 support is not ideal for now.
        QDir soundsSf2Subfolder(dir.filePath("sounds/sf2"));
        if (soundsSf2Subfolder.exists()) {
            qDebug() << soundsSf2Subfolder.absolutePath();
            soundsSf2Subfolder.setNameFilters(QStringList{"*.sf2"});
            soundsSf2Subfolder.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);
            results.append(soundsSf2Subfolder.entryInfoList());
        }
    }

    m_detectedSoundfontsMenu = new QMenu();
    ui->actionDetectedSoundFonts->setMenu(m_detectedSoundfontsMenu);

    if (results.isEmpty()) {
        m_detectedSoundfontsMenu->setEnabled(false);
        return false;
    }

    for (const QFileInfo & result : std::as_const(results)) {
        QAction * entry = m_detectedSoundfontsMenu->addAction(result.fileName());
        entry->setData(result.absoluteFilePath());
        connect(entry, &QAction::triggered, this, &MainWindow::loadSpecificSoundFontActionTriggered);
    }

    return true;
}

void MainWindow::generateThemeMenu()
{
    m_themes = new QMenu();
    ui->actionSetTheme->setMenu(m_themes);

    const QStringList & themes = QStyleFactory::keys();
    for (const QString & theme : themes) {
        QAction * entry = m_themes->addAction(theme);
        entry->setData(theme);
        connect(entry, &QAction::triggered, this, [this](){
            QAction * action = qobject_cast<QAction*>(QObject::sender());
            qApp->setStyle(action->data().toString());
            Settings::instance()->setApplicationStyle(action->data().toString());
        });
    }
}

bool MainWindow::updateFallbackSoundFontAction(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists()) return false;

    Settings::instance()->setFallbackSoundFont(info.absoluteFilePath());
    ui->actionFallbackSoundFont->setEnabled(true);
    ui->actionFallbackSoundFont->setText(info.fileName());
    ui->actionFallbackSoundFont->setData(info.absoluteFilePath());

    return true;
}

void MainWindow::playerPlaybackCallback(unsigned int curMs)
{
    ui->seekSlider->setValue(curMs);
    ui->curTimeLabel->setText(QTime::fromMSecsSinceStartOfDay(0).addMSecs(curMs).toString("m:ss").toLatin1().data());
}


void MainWindow::on_seekSlider_sliderReleased()
{
    Player::instance()->seekTo(ui->seekSlider->value());
}

void MainWindow::on_volumeSlider_valueChanged(int value)
{
    Player::instance()->setVolume(ui->volumeSlider->value() / 100.0f);
}

void MainWindow::on_actionExit_triggered()
{
    qApp->exit();
}


void MainWindow::on_action_Open_triggered()
{
    const QList<QUrl> &urls = QFileDialog::getOpenFileUrls(this, tr("Open..."), QString(), "MIDI (*.mid);;SoundFont (*.sf2 *.sf3);;All files (*.*)");
    tryLoadFiles(urls, ui->actionPlayOnDrop, true);
}


void MainWindow::on_actionSelectSoundFont_triggered()
{
    const QList<QUrl> &urls = QFileDialog::getOpenFileUrls(this, tr("Select SoundFont..."), QString(), "SoundFont (*.sf2 *.sf3)");
    tryLoadFiles(urls, false);
}


void MainWindow::on_actionHelp_triggered()
{
    const QString helpText = tr("Pineapple MIDI Player is a simple SoundFont MIDI player, which requires both MIDI file and SoundFont file to play.")
                           % QStringLiteral("\n\n")
                           % tr("You can simply drag and drop SoundFont or MIDI file to quickly load/replace a SoundFont or play the given MIDI file.")
                           % QStringLiteral("\n\n")
                           % tr("When trying to load a MIDI file, this player will try to load the SoundFont file with the same file name as the MIDI file by default, "
                                "which is suitable for playing MIDI file extracted by, for example, [VGMTrans](https://github.com/vgmtrans/vgmtrans/).")
                           % QStringLiteral("\n\n")
                           % tr("If you don't know where to get a SoundFont, check out <a href='https://musescore.org/en/handbook/3/soundfonts-and-sfz-files#list'>this page</a> provided by MuseScore.")
                           % QStringLiteral("\n\n")
                           % tr("To be clear, this player is (currently) not intended to support all features in a MIDI or SoundFont file. If you want "
                                "a more advanced MIDI player, consider try [QMidiPlayer](https://chrisoft.org/QMidiPlayer/) instead.");

    QMessageBox helpBox(this);
    helpBox.setIcon(QMessageBox::Information);
    helpBox.setWindowTitle(tr("Help"));
    helpBox.setText(helpText);
    helpBox.setTextFormat(Qt::MarkdownText);
    helpBox.exec();
}

void MainWindow::on_actionAbout_triggered()
{
    const QString mitLicense(QStringLiteral(R"(Expat/MIT License

Copyright &copy; 2024 BLumia

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
)"));

    QMessageBox infoBox(this);
    infoBox.setIcon(QMessageBox::Information);
    infoBox.setIconPixmap(QPixmap(":/icons/assets/icons/app-icon.svg"));
    infoBox.setWindowTitle(tr("About"));
    infoBox.setStandardButtons(QMessageBox::Ok);
    QPushButton * btn = infoBox.addButton(tr("License"), QMessageBox::ActionRole);
    connect(btn, &QPushButton::clicked, this, [this, &mitLicense](){
        QMessageBox licenseBox(this);
        licenseBox.setIcon(QMessageBox::Information);
        licenseBox.setWindowTitle(tr("License"));
        licenseBox.setText(mitLicense);
        licenseBox.setTextFormat(Qt::MarkdownText);
        licenseBox.exec();
    });
    infoBox.setText(
        "Pineapple MIDI Player " PMIDI_VERSION_STRING
        "\n\n" %
        tr("Based on the following free software libraries:") %
        "\n\n" %
        QStringLiteral("- [Qt](https://www.qt.io/) %1\n").arg(QT_VERSION_STR) %
        QStringLiteral("- [PortAudio](https://www.portaudio.com/) %1.%2.%3\n").arg(Pa_GetVersionInfo()->versionMajor)
                                                                              .arg(Pa_GetVersionInfo()->versionMinor)
                                                                              .arg(Pa_GetVersionInfo()->versionSubMinor) %
        "- `tsf.h` and `tml.h` from [TinySoundFont](https://github.com/schellingb/TinySoundFont/)\n"
        "- `dr_wav.h` from [dr_libs](https://github.com/mackron/dr_libs)\n"
        "- `stb_vorbis.c` from [stb](https://github.com/nothings/stb)\n"
        "- `opl.h` from [dos-like](https://github.com/mattiasgustavsson/dos-like)\n"
#ifdef Q_OS_WIN
        "- `widgetsopenwithhandler_win.cpp` from [KIO](https://invent.kde.org/frameworks/kio)\n"
#endif
#ifdef HAVE_KIO
        % QStringLiteral("- [KIO](https://invent.kde.org/frameworks/kio) %1\n").arg(KIO_VERSION_STRING) %
#endif
        "\n"
        "[Source Code](https://github.com/BLumia/pineapple-midi-player)\n"
        "\n"
        "Copyright &copy; 2025 [BLumia](https://github.com/BLumia/)"
    );
    infoBox.setTextFormat(Qt::MarkdownText);
    infoBox.exec();
}

void MainWindow::on_actionRepeat_triggered()
{
    Player::instance()->setLoop(ui->actionRepeat->isChecked());
}

void MainWindow::on_actionStayOnTop_triggered()
{
    setWindowFlag(Qt::WindowStaysOnTopHint, ui->actionStayOnTop->isChecked());
    Settings::instance()->setStayOnTop(ui->actionStayOnTop->isChecked());
    show();
}

// this action won't get triggered if HAVE_KIO is defined. Menu actions provided by KIO
// is lived as a submenu of this action.
void MainWindow::on_actionOpenWith_triggered()
{
    if (m_currentMidiFilePath.isEmpty()) return;
#ifdef Q_OS_WIN
    displayNativeOpenWithDialog({QUrl::fromLocalFile(m_currentMidiFilePath)}, this);
#else
    QMessageBox::information(this, tr("Feature not available"),
                             tr("Consider build against <a href='https://invent.kde.org/frameworks/kio'>KIO</a> to use this feature."));
#endif
}

void MainWindow::on_actionSelectFallbackSoundFont_triggered()
{
    const QString &sfpath = QFileDialog::getOpenFileName(this, tr("Select SoundFont..."), QString(), "SoundFont (*.sf2 *.sf3)");
    updateFallbackSoundFontAction(sfpath);
}

void MainWindow::on_playlistView_activated(const QModelIndex &index)
{
    m_playlistManager->setCurrentIndex(index);
    tryLoadFiles({m_playlistManager->urlByIndex(index)});
}

void MainWindow::on_actionTogglePlaylist_toggled(bool visible)
{
    ui->playlistView->setVisible(visible);
    // ensure adjustSize is called after UI updated.
    QTimer::singleShot(0, this, [this](){
        adjustSize();
    });
}


void MainWindow::on_actionConvertToWav_triggered()
{
    Player::instance()->stop();

    QString path = QFileDialog::getSaveFileName(this, tr("Render to..."), QStandardPaths::writableLocation(QStandardPaths::MusicLocation), "Wave File (*.wav)");
    if (path.isEmpty()) return;

    Player::instance()->renderToWav(QFile::encodeName(path));
}

