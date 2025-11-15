// SPDX-FileCopyrightText: 2025 Gary Wang <git@blumia.net>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QDialog>

namespace Ui { class AudioSettingsDialog; }

class AudioSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AudioSettingsDialog(QWidget *parent = nullptr);

private slots:
    void applySettings();

private:
    void populateDevices();
    void populateSampleRates();
    void populateBufferSizes();

    Ui::AudioSettingsDialog *ui;
};