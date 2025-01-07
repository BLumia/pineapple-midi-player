// SPDX-FileCopyrightText: 2025 Gary Wang <git@blumia.net>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QSlider>
#include <QMouseEvent>

class SeekableSlider : public QSlider
{
    Q_OBJECT
public:
    explicit SeekableSlider(QWidget *parent = nullptr);
    ~SeekableSlider() override = default;

signals:

public slots:

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
};

