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

#include <QVTKOpenGLNativeWidget.h>

class QShowEvent;
class TableModel;

// CellMaster-style viewport: gradient background, grid floor, corner view cube, SpaceNavigator.
class VtkTableView : public QVTKOpenGLNativeWidget {
public:
    enum StandardView { Isometric = 0, Top, Bottom, Left, Right, Front, Back };

    explicit VtkTableView(QWidget* parent = nullptr);
    ~VtkTableView() override;

    void setModel(const TableModel* model);
    void resetView();

    void handleCubeClick();
    void handleCubeHover();
    void onCameraInteraction();

protected:
    void showEvent(QShowEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    struct Impl;
    Impl* d = nullptr;

    void ensureCube();
    void applyCubeTheme();
    void ensureSpaceMouse();
    void releaseSpaceMouse();
    void applySpaceMouse();
    void frameCamera(StandardView view);
    bool tableBounds(double b[6]) const;
};
