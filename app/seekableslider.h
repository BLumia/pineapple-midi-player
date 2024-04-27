// SPDX-FileCopyrightText: 2024 Gary Wang <git@blumia.net>
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
    ~SeekableSlider() = default;

signals:

public slots:

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
};

