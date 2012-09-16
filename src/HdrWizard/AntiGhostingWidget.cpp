/**
 * This file is a part of Luminance HDR package.
 * ---------------------------------------------------------------------- 
 * Copyright (C) 2012 Franco Comida
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * ---------------------------------------------------------------------- 
 *
 * @author Franco Comida <fcomida@users.sourceforge.net>
 */

#include <QDebug>

#include <QApplication>
#include <QPainter>

#include "AntiGhostingWidget.h"

AntiGhostingWidget::AntiGhostingWidget(QImage *mask, IGraphicsView *view, QGraphicsItem *parent):
    QGraphicsItem(parent), 
    m_agMask(mask),
    m_agcursorPixmap(NULL),
    m_mx(0),
    m_my(0),
    m_view(view)
{
    //set internal brush values to their default
    m_brushAddMode = true;
    setBrushSize(32);
    setBrushStrength(255);
    fillAntiGhostingCursorPixmap();
} 

AntiGhostingWidget::~AntiGhostingWidget()
{
    qDebug() << "~AntiGhostingWidget::AntiGhostingWidget";
    if (m_agcursorPixmap)
        delete m_agcursorPixmap;
}

void AntiGhostingWidget::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_timerid = this->startTimer(0);
    }
}

void AntiGhostingWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        this->killTimer(m_timerid);
    }
}

void AntiGhostingWidget::timerEvent(QTimerEvent *) 
{
    qreal scaleFactor = m_view->transform().m11();
    QPointF relativeToWidget = m_view->mapToScene(m_view->mapFromGlobal(QCursor::pos()));
    QPainter p(m_agMask);
    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(m_requestedPixmapColor, Qt::SolidPattern));
    if (!m_brushAddMode)
        p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.drawEllipse(relativeToWidget, (m_requestedPixmapSize >> 1) / scaleFactor, (m_requestedPixmapSize >> 1) / scaleFactor);
    update();
}

void AntiGhostingWidget::setBrushSize (const int newsize) {
    m_requestedPixmapSize = newsize;
    fillAntiGhostingCursorPixmap();
    setCursor(*m_agcursorPixmap);
}

void AntiGhostingWidget::setBrushMode(bool removemode) {
    m_requestedPixmapStrength *= -1;
    m_brushAddMode = !removemode;
}

void AntiGhostingWidget::setBrushStrength (const int newstrength) {
    m_requestedPixmapStrength = newstrength;
    m_requestedPixmapColor.setAlpha(qMax(60,m_requestedPixmapStrength));
    m_requestedPixmapStrength *= (!m_brushAddMode) ? -1 : 1;
}

void AntiGhostingWidget::setBrushColor (const QColor newcolor) {
    m_requestedPixmapColor = newcolor;
    fillAntiGhostingCursorPixmap();
    setCursor(*m_agcursorPixmap);
    update();
}

void AntiGhostingWidget::switchAntighostingMode(bool ag) {
    if (ag) {
        setCursor(*m_agcursorPixmap);
    } else {
        unsetCursor();
    }
}

void AntiGhostingWidget::fillAntiGhostingCursorPixmap() {
    if (m_agcursorPixmap)
        delete m_agcursorPixmap;
    m_agcursorPixmap = new QPixmap(m_requestedPixmapSize,m_requestedPixmapSize);
    m_agcursorPixmap->fill(Qt::transparent);
    QPainter painter(m_agcursorPixmap);
    painter.setPen(Qt::DashLine);
    painter.setBrush(QBrush(m_requestedPixmapColor,Qt::SolidPattern));
    painter.drawEllipse(0,0,m_requestedPixmapSize,m_requestedPixmapSize);
}

void AntiGhostingWidget::updateVertShift(int v) {
    m_my = v;
}

void AntiGhostingWidget::updateHorizShift(int h) {
    m_mx = h;
}

QRectF AntiGhostingWidget::boundingRect() const 
{
    return QRectF(QPoint(), m_agMask->size());
}

void AntiGhostingWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                QWidget *widget)
{
    painter->drawImage(QPoint(), *m_agMask);
}
