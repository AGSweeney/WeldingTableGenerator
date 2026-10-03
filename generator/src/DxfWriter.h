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

#include <QPointF>
#include <QString>

#include <vector>

class AcadWriter;

class DxfWriter {
    friend class AcadWriter;
public:
    void addPolyline(const std::vector<QPointF>& pts, const QString& layer, double ox = 0, double oy = 0);
    void addBulgePolyline(const std::vector<QPointF>& pts, const std::vector<double>& bulges, const QString& layer);
    void addCircle(double x, double y, double r, const QString& layer);
    void addText(double x, double y, double height, const QString& layer, const QString& text);
    bool save(const QString& path, QString& error) const;

private:
    struct Item {
        int kind = 0;
        QString layer;
        std::vector<QPointF> pts;
        std::vector<double> bulges;
        double x = 0;
        double y = 0;
        double r = 0;
        double h = 0;
        QString text;
    };
    std::vector<Item> m_items;
};

bool writePartDxf(const QString& path, const Part& part, QString& error);
bool writeNestDxf(const QString& path, const TableModel& model, int sheet, QString& error);
bool writeFrameDxf(const QString& path, const TableModel& model, QString& error);
