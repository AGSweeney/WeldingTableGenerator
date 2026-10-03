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

#include "AppStyle.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

namespace AppStyle {
void apply(QApplication& app) {
    if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
        app.setStyle(fusion);
    }

    QPalette pal;
    pal.setColor(QPalette::Window, QColor(0x2b, 0x2b, 0x2b));
    pal.setColor(QPalette::WindowText, QColor(0xe8, 0xe8, 0xe8));
    pal.setColor(QPalette::Base, QColor(0x2e, 0x2e, 0x2e));
    pal.setColor(QPalette::AlternateBase, QColor(0x33, 0x33, 0x33));
    pal.setColor(QPalette::Text, QColor(0xe0, 0xe0, 0xe0));
    pal.setColor(QPalette::Button, QColor(0x45, 0x45, 0x45));
    pal.setColor(QPalette::ButtonText, QColor(0xe8, 0xe8, 0xe8));
    pal.setColor(QPalette::Highlight, QColor(0x3d, 0x6b, 0x3d));
    pal.setColor(QPalette::HighlightedText, Qt::white);
    pal.setColor(QPalette::PlaceholderText, QColor(0x9a, 0x9a, 0x9a));
    pal.setColor(QPalette::ToolTipBase, QColor(0x35, 0x35, 0x35));
    pal.setColor(QPalette::ToolTipText, QColor(0xe8, 0xe8, 0xe8));
    pal.setColor(QPalette::Mid, QColor(0x3a, 0x3a, 0x3a));
    pal.setColor(QPalette::Dark, QColor(0x1e, 0x1e, 0x1e));
    pal.setColor(QPalette::Shadow, QColor(0x1a, 0x1a, 0x1a));
    app.setPalette(pal);

    app.setStyleSheet(QStringLiteral(
        "* { font-family: \"Segoe UI\", \"Arial\", sans-serif; font-size: 12px; }"
        "QMainWindow, QDialog { background-color: #2b2b2b; color: #e8e8e8; }"
        "QMenuBar { background-color: #323232; color: #e0e0e0; border-bottom: 1px solid #1e1e1e; padding: 2px 0; }"
        "QMenuBar::item { padding: 4px 10px; background: transparent; }"
        "QMenuBar::item:selected { background-color: #3d3d3d; }"
        "QMenu { background-color: #383838; color: #e8e8e8; border: 1px solid #1a1a1a; }"
        "QMenu::item:selected { background-color: #4a6fa5; }"
        "#ribbonBar { background-color: #353535; border-bottom: 1px solid #1e1e1e; }"
        "QPushButton#ribbonTool {"
        " background-color: transparent; color: #e0e0e0; border: 1px solid transparent;"
        " border-radius: 3px; padding: 6px 10px; min-width: 56px; }"
        "QPushButton#ribbonTool:hover { background-color: #454545; border-color: #555555; }"
        "QPushButton#ribbonTool:pressed { background-color: #2a5a8a; }"
        "QPushButton#ribbonTool:checked { background-color: #3a6a9a; border-color: #5a8ac0; color: #ffffff; }"
        "#canvasFrame { background-color: #1a1a1a; border: 1px solid #3a3a3a; }"
        "QDockWidget { color: #e0e0e0; }"
        "QDockWidget::title { background: #323232; padding: 6px 8px; border-bottom: 1px solid #1e1e1e; text-align: left; }"
        "#moduleSidebar, #moduleSidebar QScrollArea, #moduleSidebar QWidget { background-color: #2e2e2e; }"
        "QGroupBox { border: 1px solid #454545; border-radius: 3px; margin-top: 0.8em; padding-top: 8px; color: #e0e0e0; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; color: #9a9a9a; }"
        "#workflowStack { border: 1px solid #454545; background: #2e2e2e; }"
        "QLabel#hintLabel { color: #9a9a9a; }"
        "QStatusBar { background-color: #2a2a2a; color: #b8b8b8; border-top: 1px solid #1e1e1e; }"
        "QStatusBar QLabel { color: #b8b8b8; padding: 0 8px; }"
        "QPushButton { background-color: #454545; color: #e8e8e8; border: 1px solid #555555; border-radius: 3px; padding: 5px 12px; }"
        "QPushButton:hover { background-color: #505050; }"
        "QPushButton:pressed { background-color: #3a6a9a; }"
        "QPushButton:disabled { color: #707070; background-color: #383838; }"
        "QPushButton#generateButton { background-color: #3d6b3d; color: #ffffff; border: 1px solid #4a8f4a; font-weight: 600; padding: 8px 12px; }"
        "QPushButton#generateButton:hover { background-color: #4a8f4a; }"
        "QPushButton#generateButton:pressed { background-color: #2f5530; }"
        "QPushButton#workflowTab {"
        " background-color: #323232; color: #c8c8c8; border: 1px solid #3a3a3a; border-radius: 2px;"
        " padding: 6px 4px; min-width: 0; font-weight: 500; }"
        "QPushButton#workflowTab:hover { background-color: #454545; color: #e8e8e8; }"
        "QPushButton#workflowTab:checked { background-color: #3a6a9a; color: #ffffff; border-color: #5a8ac0; }"
        "QPushButton#workflowTab:pressed { background-color: #2a5a8a; }"
        "QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit, QPlainTextEdit {"
        " background-color: #3a3a3a; color: #e8e8e8; border: 1px solid #505050; border-radius: 2px; padding: 3px 6px;"
        " selection-background-color: #3d6b3d; selection-color: #ffffff; }"
        "QSpinBox, QDoubleSpinBox { padding-right: 22px; min-height: 22px; }"
        "QSpinBox::up-button, QDoubleSpinBox::up-button {"
        " subcontrol-origin: border; subcontrol-position: top right; width: 18px;"
        " border-left: 1px solid #505050; border-bottom: 1px solid #505050; background-color: #454545; }"
        "QSpinBox::down-button, QDoubleSpinBox::down-button {"
        " subcontrol-origin: border; subcontrol-position: bottom right; width: 18px;"
        " border-left: 1px solid #505050; background-color: #454545; }"
        "QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,"
        "QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover { background-color: #505050; }"
        "QSpinBox::up-button:pressed, QDoubleSpinBox::up-button:pressed,"
        "QSpinBox::down-button:pressed, QDoubleSpinBox::down-button:pressed { background-color: #3a6a9a; }"
        "QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {"
        " image: url(:/icons/ui/chevron-up.svg); width: 10px; height: 10px; }"
        "QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {"
        " image: url(:/icons/ui/chevron-down.svg); width: 10px; height: 10px; }"
        "QComboBox::drop-down { border-left: 1px solid #505050; background: #454545; width: 18px; }"
        "QCheckBox { color: #e0e0e0; spacing: 6px; }"
        "QScrollArea { border: none; background: #2e2e2e; }"
        "QScrollBar:vertical { background: #2e2e2e; width: 12px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #505050; min-height: 24px; border-radius: 2px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar:horizontal { background: #2e2e2e; height: 12px; margin: 0; }"
        "QScrollBar::handle:horizontal { background: #505050; min-width: 24px; border-radius: 2px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
        "QToolTip { background-color: #353535; color: #e8e8e8; border: 1px solid #555555; }"
        "QMessageBox { background-color: #353535; }"));
}
}  // namespace AppStyle
