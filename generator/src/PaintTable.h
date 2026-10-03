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

#include "TableModel.h"

#include <QRectF>
#include <QTransform>

class QPainter;
class QPointF;

enum class TableView { Plan, Profiles, Nest, Frame, Iso };

// Default three-quarter camera for the orbit view. Pitch is clamped in the widget.
inline constexpr double kOrbitYaw = -0.65;
inline constexpr double kOrbitPitch = 0.58;

void paintTable(QPainter& painter, const QRectF& port, const TableModel& model, TableView view, bool dark, double zoom,
                const QPointF& pan, double yaw = kOrbitYaw, double pitch = kOrbitPitch, int nestSheet = -1);

QTransform viewTransform(const QRectF& port, const QRectF& world, double zoom, const QPointF& pan, bool yUp);
QRectF viewWorldBounds(const TableModel& model, TableView view, int nestSheet = -1);
QPointF screenToWorld(const QRectF& port, const QRectF& world, double zoom, const QPointF& pan, const QPointF& screen);
