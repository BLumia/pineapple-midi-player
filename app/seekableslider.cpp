// SPDX-FileCopyrightText: 2024 Gary Wang <git@blumia.net>
//
// SPDX-License-Identifier: MIT

#include "seekableslider.h"

SeekableSlider::SeekableSlider(QWidget *parent) :
    QSlider(parent)
{
}

void SeekableSlider::mouseReleaseEvent(QMouseEvent *event)
{
    double pos = event->pos().x() / (double)width();
    setValue(pos * (maximum() - minimum()) + minimum());
    emit sliderReleased();
    return QSlider::mouseReleaseEvent(event);
}
