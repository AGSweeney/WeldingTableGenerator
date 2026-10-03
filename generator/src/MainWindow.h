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

#include <QMainWindow>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class PreviewWidget;
class VtkTableView;
class QStackedWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void buildUi();
    void onEdited();
    void generate();
    void wire(QDoubleSpinBox* spin);
    void wire(QSpinBox* spin);
    void wire(QCheckBox* box);
    TableSpec collect() const;
    void applySpec(const TableSpec& spec);
    void setHoleSpin(bool inches, double millimeters);
    void rebuild();
    void loadSettings();
    void saveSettings() const;
    void closeEvent(QCloseEvent* event) override;

    TableModel m_model;
    bool m_loading = false;

    PreviewWidget* m_preview = nullptr;
    VtkTableView* m_vtk = nullptr;
    QStackedWidget* m_canvas = nullptr;
    QLabel* m_summary = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_coord = nullptr;
    QLabel* m_hint = nullptr;
    QLabel* m_legLength = nullptr;
    QLabel* m_coupon = nullptr;
    QComboBox* m_preset = nullptr;
    QButtonGroup* m_views = nullptr;

    QDoubleSpinBox* m_length = nullptr;
    QDoubleSpinBox* m_width = nullptr;
    QDoubleSpinBox* m_topThickness = nullptr;
    QDoubleSpinBox* m_webThickness = nullptr;
    QDoubleSpinBox* m_webDepth = nullptr;
    QDoubleSpinBox* m_holeMm = nullptr;
    QComboBox* m_holeUnit = nullptr;
    QDoubleSpinBox* m_pitchX = nullptr;
    QDoubleSpinBox* m_pitchY = nullptr;
    QDoubleSpinBox* m_marginX = nullptr;
    QDoubleSpinBox* m_marginY = nullptr;
    QCheckBox* m_square = nullptr;
    QDoubleSpinBox* m_apronInset = nullptr;
    QCheckBox* m_apronTopSlots = nullptr;
    QDoubleSpinBox* m_clearance = nullptr;
    QDoubleSpinBox* m_tabWidth = nullptr;
    QDoubleSpinBox* m_slotExtra = nullptr;
    QDoubleSpinBox* m_tabHeight = nullptr;
    QDoubleSpinBox* m_endTabLength = nullptr;
    QDoubleSpinBox* m_endTabHeight = nullptr;
    QDoubleSpinBox* m_ribGap = nullptr;
    QDoubleSpinBox* m_halfLap = nullptr;
    QCheckBox* m_apronHoles = nullptr;
    QDoubleSpinBox* m_holeFromTop = nullptr;
    QDoubleSpinBox* m_holeFromBottom = nullptr;
    QDoubleSpinBox* m_crossSpacing = nullptr;
    QDoubleSpinBox* m_longSpacing = nullptr;
    QCheckBox* m_lightening = nullptr;
    QDoubleSpinBox* m_openingHeight = nullptr;
    QDoubleSpinBox* m_landWide = nullptr;
    QDoubleSpinBox* m_landNarrow = nullptr;
    QDoubleSpinBox* m_landThreshold = nullptr;
    QDoubleSpinBox* m_openingGap = nullptr;
    QDoubleSpinBox* m_openingMax = nullptr;
    QDoubleSpinBox* m_openingMin = nullptr;
    QDoubleSpinBox* m_openingSnap = nullptr;
    QCheckBox* m_frame = nullptr;
    QDoubleSpinBox* m_tube = nullptr;
    QDoubleSpinBox* m_stringer = nullptr;
    QDoubleSpinBox* m_stringerHeight = nullptr;
    QCheckBox* m_doubleStringers = nullptr;
    QDoubleSpinBox* m_tubeWall = nullptr;
    QSpinBox* m_supports = nullptr;
    QDoubleSpinBox* m_finished = nullptr;
    QLabel* m_footNote = nullptr;
    QDoubleSpinBox* m_sheetL = nullptr;
    QDoubleSpinBox* m_sheetW = nullptr;
    QDoubleSpinBox* m_nestGap = nullptr;
    QDoubleSpinBox* m_nestMargin = nullptr;
    QCheckBox* m_nestCoupon = nullptr;
    QCheckBox* m_q02 = nullptr;
    QDoubleSpinBox* m_density = nullptr;
    QLineEdit* m_title = nullptr;
    QLineEdit* m_revision = nullptr;
    QPlainTextEdit* m_notes = nullptr;
    QCheckBox* m_writeParts = nullptr;
    QCheckBox* m_writeNest = nullptr;
    QCheckBox* m_writeFrame = nullptr;
    QCheckBox* m_writePdf = nullptr;
    QCheckBox* m_writeReadme = nullptr;
    QCheckBox* m_writeJson = nullptr;
    QLineEdit* m_output = nullptr;
    QCheckBox* m_openFolder = nullptr;
};
