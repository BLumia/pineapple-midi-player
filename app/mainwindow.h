// SPDX-FileCopyrightText: 2024 Gary Wang <git@blumia.net>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    bool loadMidiFile(const QString & path);
    void loadSoundFontFile(const QString & path);
    void loadOP2File(const QString & path);
    void tryLoadFiles(const QList<QUrl> &urls, bool tryPlayAfterLoad = true, bool suppressWarningDlg = false);
    bool checkCanPlay(bool suppressWarningDlg = false);
    void tryPlay(bool suppressWarningDlg = false);

    static QList<QUrl> convertToUrlList(const QStringList &files);

signals:
    void midiFileLoaded(const QString path);
    void soundfontLoaded(const QString path);

private slots:
    void loadSpecificSoundFontActionTriggered();

    void on_playBtn_clicked();
    void on_stopBtn_clicked();
    void on_seekSlider_sliderReleased();
    void on_volumeSlider_valueChanged(int value);
    void on_actionExit_triggered();
    void on_action_Open_triggered();
    void on_actionSelectSoundFont_triggered();
    void on_actionHelp_triggered();
    void on_actionAbout_triggered();
    void on_renderBtn_clicked();
    void on_actionRepeat_triggered();
    void on_actionStayOnTop_triggered();
    void on_actionOpenWith_triggered();
    void on_actionSelectFallbackSoundFont_triggered();

private:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    bool ensureWeHaveSoundfont();
    bool scanSoundfonts();
    void generateThemeMenu();
    bool updateFallbackSoundFontAction(const QString & path);

    // callbacks
    void playerPlaybackCallback(unsigned int curMs);

    int m_noSoundFontInfoBoxShownTimesCount = 0;
    QString m_currentSf2FilePath; //< Only set this value when SF2 file actually get loaded.
    QString m_currentMidiFilePath; //< Only set this value when MIDI file actually get loaded.
    QMenu *m_detectedSoundfontsMenu;
    QMenu *m_themes;

    Ui::MainWindow *ui;
};
