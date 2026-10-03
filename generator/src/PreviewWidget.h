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

#pragma once

#include "PaintTable.h"

#include <QWidget>

#include <functional>

class PreviewWidget : public QWidget {
public:
    explicit PreviewWidget(QWidget* parent = nullptr);
    void setModel(const TableModel* model);
    void setView(TableView view);
    TableView view() const { return m_view; }
    void fit();
    void setCoordCallback(std::function<void(const QString&)> cb) { m_coord = std::move(cb); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    enum class DragMode { None, Pan, Orbit };

    const TableModel* m_model = nullptr;
    TableView m_view = TableView::Plan;
    double m_zoom = 1;
    QPointF m_pan;
    double m_yaw = kOrbitYaw;
    double m_pitch = kOrbitPitch;
    DragMode m_drag = DragMode::None;
    QPointF m_dragAnchor;
    QPointF m_panAnchor;
    double m_yawAnchor = kOrbitYaw;
    double m_pitchAnchor = kOrbitPitch;
    std::function<void(const QString&)> m_coord;
};
