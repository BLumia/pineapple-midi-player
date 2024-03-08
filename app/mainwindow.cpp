#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "player.h"

#include <QDir>
#include <QDropEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QMessageBox>
#include <QFileDialog>
#include <QStringList>
#include <QStandardPaths>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_detectedSoundfontsMenu(nullptr)
{
    ui->setupUi(this);
    setAcceptDrops(true);

    adjustSize();

    scanSoundfonts();

    Player::instance()->setPlaybackCallback(std::bind(&MainWindow::playerPlaybackCallback, this, std::placeholders::_1));
    Player::instance()->onIsPlayingChanged([this](bool isPlaying){
        ui->playBtn->setText(isPlaying ? QCoreApplication::translate("MainWindow", "Pause")
                                       : QCoreApplication::translate("MainWindow", "Play"));
    });

    connect(this, &MainWindow::midiFileLoaded, this, [this](const QString path){
        QFileInfo fi(path);
        ui->midiFileLabel->setText(fi.fileName());
        m_currentMidiFilePath = path;

        auto info = Player::instance()->midiInfo();
        unsigned int duration = std::get<unsigned int>(info[Player::I_LENGTH_MS]);
        ui->seekSlider->setMaximum(duration);
        ui->durationLabel->setText(QTime::fromMSecsSinceStartOfDay(0).addMSecs(duration).toString("m:ss").toLatin1().data());
    });

    connect(this, &MainWindow::soundfontLoaded, this, [this](const QString path){
        QFileInfo fi(path);
        ui->soundFontFileLabel->setText(fi.fileName());
        m_currentSf2FilePath = path;
    });
}

MainWindow::~MainWindow()
{
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
        return false;
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

void MainWindow::tryLoadFiles(const QList<QUrl> &urls, bool tryPlayAfterLoad, bool suppressWarningDlg)
{
    const QStringList soundfontFormats{"sf2", "sf3"};

    for (const QUrl & url : urls) {
        QFileInfo fi(url.toLocalFile());
        if (fi.suffix().toLower() == "mid") {
            loadMidiFile(fi.absoluteFilePath());
        } else if (soundfontFormats.contains(fi.suffix().toLower())) {
            loadSoundFontFile(fi.absoluteFilePath());
        }
    }

    if (tryPlayAfterLoad) {
        tryPlay(suppressWarningDlg);
    }
}

bool MainWindow::checkCanPlay(bool suppressWarningDlg)
{
    if (m_currentMidiFilePath.isEmpty()) {
        if (!suppressWarningDlg) QMessageBox::information(this, "Missing MIDI file", "Please load a MIDI file first.");
        return false;
    }

    if (!ensureWeHaveSoundfont()) {
        if (!suppressWarningDlg) {
            QMessageBox infoBox(this);
            infoBox.setIcon(QMessageBox::Information);
            infoBox.setWindowTitle("Missing SoundFont");
            infoBox.setInformativeText("If you don't know where to get, check out <a href='https://musescore.org/en/handbook/3/soundfonts-and-sfz-files#list'>this page</a> provided by MuseScore.");
            infoBox.setText("You need to select a SoundFont before play the MIDI file.");
            infoBox.setTextFormat(Qt::MarkdownText);
            infoBox.exec();
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

void MainWindow::on_playBtn_clicked()
{
    if (!checkCanPlay(false)) return;
    Player::instance()->isPlaying() ? Player::instance()->pause() : Player::instance()->play();
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
        // TODO: attempt to load fallback soundfont
        return false;
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
        QDir dir(path);
        if (dir.exists() && dir.cd("soundfonts")) {
            qDebug() << dir.absolutePath();
            dir.setNameFilters(QStringList{"*.sf2", "*.sf3"});
            dir.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);
            results.append(dir.entryInfoList());
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
        connect(entry, &QAction::triggered, this, [this](){
            QAction * action = qobject_cast<QAction*>(QObject::sender());
            loadSoundFontFile(action->data().toString());
        });
    }

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
    QMessageBox::information(this, "Help", "You don't need help");
}


void MainWindow::on_actionAbout_triggered()
{
    QMessageBox infoBox(this);
    infoBox.setIcon(QMessageBox::Information);
    infoBox.setIconPixmap(QPixmap(":/icons/dist/app-icon.svg"));
    infoBox.setWindowTitle("About Pineapple MIDI Player");
    infoBox.setText(
        "Pineapple MIDI Player\n"
        "\n"
        "Copyright (C) 2024 BLumia"
    );
    infoBox.setTextFormat(Qt::MarkdownText);
    infoBox.exec();
}

void MainWindow::on_renderBtn_clicked()
{
    Player::instance()->stop();

    QString path = QFileDialog::getSaveFileName(this, "Render to...", QStandardPaths::writableLocation(QStandardPaths::MusicLocation), "Wave File (*.wav)");
    if (path.isEmpty()) return;

    Player::instance()->renderToWav(QFile::encodeName(path));
}

