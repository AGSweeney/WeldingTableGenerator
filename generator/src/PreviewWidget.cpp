/*
 * Copyright (c) 2026 Adam G. Sweeney <AGSweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "PreviewWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>

PreviewWidget::PreviewWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(480, 360);
    setAutoFillBackground(false);
}

void PreviewWidget::setModel(const TableModel* model) {
    m_model = model;
    update();
}

void PreviewWidget::setView(TableView view) {
    m_view = view;
    fit();
    if (m_view == TableView::Iso) {
        setCursor(Qt::OpenHandCursor);
        if (m_coord) {
            m_coord(QStringLiteral("Drag to spin. Right-drag to pan."));
        }
    } else {
        unsetCursor();
    }
}

void PreviewWidget::fit() {
    m_zoom = 1;
    m_pan = {};
    m_yaw = kOrbitYaw;
    m_pitch = kOrbitPitch;
    update();
}

void PreviewWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (!m_model || !m_model->errors().isEmpty() && m_model->parts().empty()) {
        painter.fillRect(rect(), QColor(0xe8, 0xea, 0xed));
        painter.setPen(QColor(0x2b, 0x2b, 0x2b));
        painter.drawText(rect().adjusted(24, 24, -24, -24), Qt::AlignCenter | Qt::TextWordWrap,
                         m_model && !m_model->errors().isEmpty() ? m_model->errors().join(QStringLiteral("\n"))
                                                                 : QStringLiteral("Adjust the plate settings to preview the table."));
        return;
    }
    paintTable(painter, QRectF(rect()), *m_model, m_view, true, m_zoom, m_pan, m_yaw, m_pitch);
}

void PreviewWidget::wheelEvent(QWheelEvent* event) {
    if (!m_model || m_model->parts().empty()) {
        return;
    }
    const QRectF world = viewWorldBounds(*m_model, m_view);
    const QPointF cursor = event->position();
    const bool yUp = m_view != TableView::Profiles;
    const QPointF before = viewTransform(QRectF(rect()), world, m_zoom, m_pan, yUp).inverted().map(cursor);
    const double factor = event->angleDelta().y() >= 0 ? 1.12 : 1.0 / 1.12;
    m_zoom = std::clamp(m_zoom * factor, 0.25, 40.0);
    const QPointF screen = viewTransform(QRectF(rect()), world, m_zoom, m_pan, yUp).map(before);
    m_pan += cursor - screen;
    update();
    event->accept();
}

void PreviewWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_drag = m_view == TableView::Iso ? DragMode::Orbit : DragMode::Pan;
        m_dragAnchor = event->position();
        m_panAnchor = m_pan;
        m_yawAnchor = m_yaw;
        m_pitchAnchor = m_pitch;
        setCursor(m_drag == DragMode::Orbit ? Qt::ClosedHandCursor : Qt::SizeAllCursor);
    } else if (event->button() == Qt::RightButton && m_view == TableView::Iso) {
        m_drag = DragMode::Pan;
        m_dragAnchor = event->position();
        m_panAnchor = m_pan;
        setCursor(Qt::SizeAllCursor);
    }
}

void PreviewWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_drag == DragMode::Orbit) {
        const QPointF d = event->position() - m_dragAnchor;
        m_yaw = m_yawAnchor + d.x() * 0.012;
        m_pitch = std::clamp(m_pitchAnchor - d.y() * 0.008, -1.05, 1.25);
        update();
    } else if (m_drag == DragMode::Pan) {
        m_pan = m_panAnchor + (event->position() - m_dragAnchor);
        update();
    }
    if (!m_coord || !m_model || m_model->parts().empty()) {
        return;
    }
    if (m_view == TableView::Iso) {
        m_coord(QStringLiteral("Drag to spin. Right-drag to pan."));
        return;
    }
    if (m_view == TableView::Profiles) {
        m_coord(QStringLiteral("Part profiles"));
        return;
    }
    const QPointF w = screenToWorld(QRectF(rect()), viewWorldBounds(*m_model, m_view), m_zoom, m_pan, event->position());
    m_coord(QStringLiteral("X %1 in    Y %2 in").arg(QString::number(w.x(), 'f', 3), QString::number(w.y(), 'f', 3)));
}

void PreviewWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
        m_drag = DragMode::None;
        if (m_view == TableView::Iso) {
            setCursor(Qt::OpenHandCursor);
        } else {
            unsetCursor();
        }
    }
}
