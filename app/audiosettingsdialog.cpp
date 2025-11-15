// SPDX-FileCopyrightText: 2025 Gary Wang <git@blumia.net>
//
// SPDX-License-Identifier: MIT

#include "audiosettingsdialog.h"

#include "ui_audiosettingsdialog.h"

#include "player.h"
#include "settings.h"

AudioSettingsDialog::AudioSettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AudioSettingsDialog)
{
    ui->setupUi(this);
    populateDevices();
    populateSampleRates();
    populateBufferSizes();
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &AudioSettingsDialog::applySettings);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &AudioSettingsDialog::reject);
}

void AudioSettingsDialog::populateDevices()
{
    ui->deviceCombo->clear();
    ui->deviceCombo->addItem(tr("Auto"), -1);
    const auto devices = Player::instance()->enumerateOutputDevices();
    int currentIndex = -1;
    const auto cur = Player::instance()->currentAudioSettings();
    for (const auto &d : devices) {
        const QString text = QString::fromStdString(d.name) + QStringLiteral(" (") + QString::fromStdString(d.hostApi) + QStringLiteral(")");
        ui->deviceCombo->addItem(text, d.index);
        if (d.index == cur.deviceIndex) {
            currentIndex = ui->deviceCombo->count() - 1;
        }
    }
    if (currentIndex >= 0) ui->deviceCombo->setCurrentIndex(currentIndex);
}

void AudioSettingsDialog::populateSampleRates()
{
    ui->sampleRateCombo->clear();
    const QList<int> rates{44100, 48000, 88200, 96000};
    const auto cur = Player::instance()->currentAudioSettings();
    int currentIndex = -1;
    for (int r : rates) {
        ui->sampleRateCombo->addItem(QString::number(r), r);
        if (r == cur.sampleRate) currentIndex = ui->sampleRateCombo->count() - 1;
    }
    if (currentIndex >= 0) ui->sampleRateCombo->setCurrentIndex(currentIndex);
}

void AudioSettingsDialog::populateBufferSizes()
{
    ui->bufferSizeCombo->clear();
    struct Item { QString label; int value; };
    const Item items[]{{tr("Auto"), 0}, {QStringLiteral("64"), 64}, {QStringLiteral("128"), 128}, {QStringLiteral("256"), 256}, {QStringLiteral("512"), 512}, {QStringLiteral("1024"), 1024}};
    const auto cur = Player::instance()->currentAudioSettings();
    int currentIndex = -1;
    for (const auto &it : items) {
        ui->bufferSizeCombo->addItem(it.label, it.value);
        if ((unsigned long)it.value == cur.framesPerBuffer) currentIndex = ui->bufferSizeCombo->count() - 1;
    }
    if (currentIndex >= 0) ui->bufferSizeCombo->setCurrentIndex(currentIndex);
}

void AudioSettingsDialog::applySettings()
{
    Player::AudioSettings s;
    s.deviceIndex = ui->deviceCombo->currentData().toInt();
    s.sampleRate = ui->sampleRateCombo->currentData().toInt();
    s.channels = 2;
    s.framesPerBuffer = (unsigned long)ui->bufferSizeCombo->currentData().toInt();
    s.suggestedLatency = 0.0;

    if (Player::instance()->applyAudioSettings(s)) {
        Settings::instance()->setAudioDeviceIndex(s.deviceIndex);
        Settings::instance()->setAudioSampleRate(s.sampleRate);
        Settings::instance()->setAudioFramesPerBuffer((int)s.framesPerBuffer);
        accept();
    } else {
        reject();
    }
}
