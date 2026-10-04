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

#include "MainWindow.h"

#include "PackageWriter.h"
#include "PreviewWidget.h"
#include "VtkTableView.h"

#include <algorithm>

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QOverload>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStatusBar>
#include <QSpinBox>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QDoubleSpinBox* numSpin(double min, double max, double value, int decimals, double step, const QString& suffix,
                        const QString& tip) {
    auto* spin = new QDoubleSpinBox();
    spin->setRange(min, max);
    spin->setDecimals(decimals);
    spin->setValue(value);
    spin->setSingleStep(step);
    spin->setSuffix(suffix);
    spin->setToolTip(tip);
    spin->setAlignment(Qt::AlignRight);
    spin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    spin->setMinimumWidth(148);
    return spin;
}

QFormLayout* formOf(QWidget* page) {
    auto* form = new QFormLayout(page);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setContentsMargins(10, 10, 10, 10);
    form->setVerticalSpacing(6);
    return form;
}

struct SectionTabs {
    QButtonGroup* group = nullptr;
    QStackedWidget* stack = nullptr;
    QGridLayout* grid = nullptr;
    int count = 0;
};

QWidget* addPage(SectionTabs& sections, const QString& title, const QString& tip) {
    auto* button = new QPushButton(title);
    button->setObjectName(QStringLiteral("workflowTab"));
    button->setCheckable(true);
    button->setToolTip(tip);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    const int index = sections.count++;
    sections.grid->addWidget(button, index / 4, index % 4);
    sections.group->addButton(button, index);
    if (index == 0) {
        button->setChecked(true);
    }

    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* page = new QWidget();
    scroll->setWidget(page);
    sections.stack->addWidget(scroll);
    return page;
}

QCheckBox* check(const QString& text, bool on, const QString& tip) {
    auto* box = new QCheckBox(text);
    box->setChecked(on);
    box->setToolTip(tip);
    return box;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    buildUi();
    loadSettings();
    rebuild();
}

void MainWindow::wire(QDoubleSpinBox* spin) {
    connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { onEdited(); });
}

void MainWindow::wire(QSpinBox* spin) {
    connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { onEdited(); });
}

void MainWindow::wire(QCheckBox* box) {
    connect(box, &QCheckBox::toggled, this, [this](bool) { onEdited(); });
}

void MainWindow::buildUi() {
    setWindowTitle(QStringLiteral("Welding Table Generator"));
    auto* file = menuBar()->addMenu(QStringLiteral("File"));
    file->addAction(QStringLiteral("Save settings..."), this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save settings"), m_output->text(),
                                                          QStringLiteral("JSON (*.json)"));
        if (path.isEmpty()) {
            return;
        }
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(QJsonDocument(collect().toJson()).toJson(QJsonDocument::Indented));
        }
    });
    file->addAction(QStringLiteral("Load settings..."), this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Load settings"), m_output->text(),
                                                          QStringLiteral("JSON (*.json)"));
        if (path.isEmpty()) {
            return;
        }
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (!doc.isObject()) {
            QMessageBox::warning(this, QStringLiteral("Settings"), QStringLiteral("That file is not a settings object."));
            return;
        }
        m_loading = true;
        applySpec(TableSpec::fromJson(doc.object(), collect()));
        m_preset->setCurrentIndex(3);
        m_loading = false;
        rebuild();
    });
    file->addSeparator();
    file->addAction(QStringLiteral("Generate package"), this, [this] { generate(); });
    file->addSeparator();
    file->addAction(QStringLiteral("Exit"), this, &QWidget::close);
    menuBar()->addMenu(QStringLiteral("Help"))->addAction(QStringLiteral("About"), this, [this] {
        QMessageBox::information(
            this, QStringLiteral("Welding Table Generator"),
            QStringLiteral("Builds laser-cut DXF parts, a sheet nest, a leg plan, and a fabrication PDF.\n\n"
                           "The window matches the other shop tools: workflow settings on the left, live preview on the right.\n\n"
                           "Defaults reproduce the 24 x 116 Rev A table. DXF units are inches. Cut only CUT_OUTER and CUT_INNER.\n\n"
                           "Scroll the preview with the wheel. Drag to pan. Fit returns to the whole view."));
    });

    auto* central = new QWidget(this);
    auto* centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    auto* ribbon = new QWidget(central);
    ribbon->setObjectName(QStringLiteral("ribbonBar"));
    auto* viewRow = new QHBoxLayout(ribbon);
    viewRow->setContentsMargins(8, 4, 8, 4);
    m_views = new QButtonGroup(this);
    m_views->setExclusive(true);
    const struct {
        const char* label;
        TableView view;
    } views[] = {{"Plan", TableView::Plan}, {"Ribs", TableView::Profiles}, {"Nest", TableView::Nest},
                 {"Frame", TableView::Frame}, {"3D", TableView::Iso}};
    for (int i = 0; i < 5; ++i) {
        auto* button = new QPushButton(QString::fromLatin1(views[i].label));
        button->setObjectName(QStringLiteral("ribbonTool"));
        if (views[i].view == TableView::Iso) {
            button->setToolTip(QStringLiteral(
                "Orbit the table on the grid. Drag to spin, wheel to zoom, middle-drag to pan. "
                "Click the corner cube for a standard view. SpaceNavigator orbits the camera. Fit returns to isometric."));
        }
        button->setCheckable(true);
        button->setChecked(i == 0);
        m_views->addButton(button, i);
        viewRow->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, view = views[i].view] {
            if (view == TableView::Iso) {
                m_canvas->setCurrentWidget(m_vtk);
                m_coord->setText(QStringLiteral("Drag to orbit. Click the cube for a view. SpaceNavigator orbits."));
            } else {
                m_canvas->setCurrentWidget(m_preview);
                m_preview->setView(view);
            }
        });
    }
    auto* fit = new QPushButton(QStringLiteral("Fit"));
    fit->setObjectName(QStringLiteral("ribbonTool"));
    fit->setToolTip(QStringLiteral("Fit the preview. On the 3D view this returns to isometric."));
    viewRow->addWidget(fit);
    viewRow->addStretch(1);
    centralLayout->addWidget(ribbon);

    auto* canvasFrame = new QFrame(central);
    canvasFrame->setObjectName(QStringLiteral("canvasFrame"));
    auto* canvasLayout = new QVBoxLayout(canvasFrame);
    canvasLayout->setContentsMargins(1, 1, 1, 1);
    m_canvas = new QStackedWidget(canvasFrame);
    m_preview = new PreviewWidget(m_canvas);
    m_preview->setCoordCallback([this](const QString& text) { m_coord->setText(text); });
    m_vtk = new VtkTableView(m_canvas);
    m_canvas->addWidget(m_preview);
    m_canvas->addWidget(m_vtk);
    canvasLayout->addWidget(m_canvas);
    centralLayout->addWidget(canvasFrame, 1);
    connect(fit, &QPushButton::clicked, this, [this] {
        if (m_canvas->currentWidget() == m_vtk) {
            m_vtk->resetView();
        } else {
            m_preview->fit();
        }
    });

    m_summary = new QLabel();
    m_summary->setWordWrap(false);
    m_status = new QLabel(QStringLiteral("Ready."));
    m_coord = new QLabel(QStringLiteral("Move over the preview for coordinates"));
    statusBar()->addWidget(m_summary, 1);
    statusBar()->addPermanentWidget(m_coord);
    statusBar()->addPermanentWidget(m_status);
    setCentralWidget(central);

    auto* dock = new QDockWidget(QStringLiteral("Workflow"), this);
    dock->setObjectName(QStringLiteral("moduleSidebar"));
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    dock->setMinimumWidth(420);
    auto* dockBody = new QWidget();
    auto* dockLayout = new QVBoxLayout(dockBody);
    dockLayout->setContentsMargins(8, 8, 8, 8);

    m_preset = new QComboBox();
    m_preset->addItems({QStringLiteral("24 x 116 Rev A"), QStringLiteral("36 x 72"), QStringLiteral("48 x 96"),
                        QStringLiteral("Custom")});
    m_preset->setToolTip(QStringLiteral("Starting sizes. Editing any field switches this to Custom."));
    dockLayout->addWidget(m_preset);
    connect(m_preset, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (m_loading || index == 3) {
            return;
        }
        TableSpec spec = TableSpec::revA();
        if (index == 1) {
            spec.width = 36;
            spec.length = 72;
        } else if (index == 2) {
            spec.width = 48;
            spec.length = 96;
        }
        m_loading = true;
        applySpec(spec);
        m_loading = false;
        rebuild();
    });

    auto* sectionHost = new QWidget();
    auto* sectionLayout = new QVBoxLayout(sectionHost);
    sectionLayout->setContentsMargins(0, 0, 0, 0);
    sectionLayout->setSpacing(6);
    SectionTabs sections;
    sections.group = new QButtonGroup(this);
    sections.group->setExclusive(true);
    sections.stack = new QStackedWidget();
    sections.stack->setObjectName(QStringLiteral("workflowStack"));
    sections.grid = new QGridLayout();
    sections.grid->setContentsMargins(0, 0, 0, 0);
    sections.grid->setSpacing(4);
    connect(sections.group, &QButtonGroup::idClicked, sections.stack, &QStackedWidget::setCurrentIndex);
    sectionLayout->addLayout(sections.grid);
    sectionLayout->addWidget(sections.stack, 1);

    auto* plate = addPage(sections, QStringLiteral("Plate"), QStringLiteral("Top size and stock thickness."));
    auto* plateForm = formOf(plate);
    m_length = numSpin(8, 480, 116, 3, 1, QStringLiteral(" in"), QStringLiteral("Overall top length, along X."));
    m_width = numSpin(8, 144, 24, 3, 1, QStringLiteral(" in"), QStringLiteral("Overall top width, along Y."));
    m_topThickness = numSpin(0.125, 2, 0.375, 4, 0.0625, QStringLiteral(" in"),
                             QStringLiteral("Top plate thickness. Tabs must stay shorter than this."));
    m_webThickness = numSpin(0.06, 1, 0.236, 4, 0.001, QStringLiteral(" in"),
                             QStringLiteral("Apron and rib stock thickness. Slot width is this plus the clearance."));
    m_webDepth = numSpin(3, 24, 6, 3, 0.25, QStringLiteral(" in"),
                         QStringLiteral("Apron and rib depth, from the underside of the top downward."));
    plateForm->addRow(QStringLiteral("Length"), m_length);
    plateForm->addRow(QStringLiteral("Width"), m_width);
    plateForm->addRow(QStringLiteral("Top thickness"), m_topThickness);
    plateForm->addRow(QStringLiteral("Web thickness"), m_webThickness);
    plateForm->addRow(QStringLiteral("Web depth"), m_webDepth);

    auto* holes = addPage(sections, QStringLiteral("Holes"), QStringLiteral("Dog-hole diameter, pitch, and margins."));
    auto* holeForm = formOf(holes);
    m_holeMm = numSpin(4, 50, 16, 3, 0.5, QStringLiteral(" mm"),
                       QStringLiteral("Dog-hole diameter. Choose mm or inches beside this field. 16 mm is 0.6299 in."));
    m_holeUnit = new QComboBox();
    m_holeUnit->addItem(QStringLiteral("mm"));
    m_holeUnit->addItem(QStringLiteral("in"));
    m_holeUnit->setToolTip(QStringLiteral("Units for the dog-hole diameter. Switching converts the current size."));
    auto* holeRow = new QWidget();
    auto* holeLay = new QHBoxLayout(holeRow);
    holeLay->setContentsMargins(0, 0, 0, 0);
    holeLay->addWidget(m_holeMm, 1);
    holeLay->addWidget(m_holeUnit);
    m_pitchX = numSpin(0.5, 24, 4, 3, 0.5, QStringLiteral(" in"), QStringLiteral("Hole spacing along the length."));
    m_pitchY = numSpin(0.5, 24, 4, 3, 0.5, QStringLiteral(" in"), QStringLiteral("Hole spacing across the width."));
    m_marginX = numSpin(0, 24, 2, 3, 0.25, QStringLiteral(" in"), QStringLiteral("First and last hole inset from the ends."));
    m_marginY = numSpin(0, 24, 2, 3, 0.25, QStringLiteral(" in"), QStringLiteral("First and last hole inset from the front and back."));
    m_square = check(QStringLiteral("Square grid"), true, QStringLiteral("Keep Y pitch and Y margin locked to X."));
    holeForm->addRow(QStringLiteral("Diameter"), holeRow);
    holeForm->addRow(QStringLiteral("Pitch X"), m_pitchX);
    holeForm->addRow(QStringLiteral("Pitch Y"), m_pitchY);
    holeForm->addRow(QStringLiteral("Margin X"), m_marginX);
    holeForm->addRow(QStringLiteral("Margin Y"), m_marginY);
    holeForm->addRow(QString(), m_square);

    auto* joints = addPage(sections, QStringLiteral("Joints"), QStringLiteral("Tabs, slots, aprons, and half-laps."));
    auto* tabForm = formOf(joints);
    m_apronInset = numSpin(0, 6, 0.5, 3, 0.125, QStringLiteral(" in"),
                           QStringLiteral("Apron outer face inset from the edge of the top."));
    m_clearance = numSpin(0, 0.1, 0.010, 4, 0.001, QStringLiteral(" in"),
                          QStringLiteral("Total extra slot width over the web thickness."));
    m_tabWidth = numSpin(0.25, 3, 1.0, 3, 0.125, QStringLiteral(" in"), QStringLiteral("Top tab width along the rib."));
    m_slotExtra = numSpin(0, 0.25, 0.020, 4, 0.005, QStringLiteral(" in"),
                          QStringLiteral("Total extra slot length over the tab or end tab."));
    m_tabHeight = numSpin(0.05, 1.5, 0.300, 4, 0.025, QStringLiteral(" in"),
                          QStringLiteral("How far top tabs project into the top plate."));
    m_endTabLength = numSpin(0.05, 1, 0.200, 4, 0.025, QStringLiteral(" in"),
                             QStringLiteral("Rib end tab projection through the apron."));
    m_endTabHeight = numSpin(0.25, 4, 1.0, 3, 0.125, QStringLiteral(" in"),
                             QStringLiteral("Rib end tab height, centered on the web depth."));
    m_ribGap = numSpin(0, 0.25, 0.010, 4, 0.005, QStringLiteral(" in"),
                       QStringLiteral("Gap between a rib end and the inner face of the apron."));
    m_halfLap = numSpin(0, 0.1, 0.005, 4, 0.001, QStringLiteral(" in"),
                        QStringLiteral("Each half-lap passes mid-depth by this amount. Total clearance is double."));
    m_apronTopSlots = check(QStringLiteral("Apron slots in the top"), true,
                            QStringLiteral("Cut a top slot for each apron tab. When the apron is flush with the edge, the slot opens through the plate edge. Turn off to omit those tabs and slots."));
    m_apronHoles = check(QStringLiteral("Apron dog holes"), true,
                         QStringLiteral("Two rows of dog holes in the long and end aprons."));
    m_holeFromTop = numSpin(0.25, 6, 1, 3, 0.25, QStringLiteral(" in"),
                            QStringLiteral("Apron hole distance below the top underside."));
    m_holeFromBottom = numSpin(0.25, 6, 1, 3, 0.25, QStringLiteral(" in"),
                               QStringLiteral("Apron hole distance above the bottom edge of the web."));
    tabForm->addRow(QStringLiteral("Apron inset"), m_apronInset);
    tabForm->addRow(QString(), m_apronTopSlots);
    tabForm->addRow(QStringLiteral("Slot clearance"), m_clearance);
    tabForm->addRow(QStringLiteral("Tab width"), m_tabWidth);
    tabForm->addRow(QStringLiteral("Extra slot length"), m_slotExtra);
    tabForm->addRow(QStringLiteral("Top tab height"), m_tabHeight);
    tabForm->addRow(QStringLiteral("End tab length"), m_endTabLength);
    tabForm->addRow(QStringLiteral("End tab height"), m_endTabHeight);
    tabForm->addRow(QStringLiteral("Rib end gap"), m_ribGap);
    tabForm->addRow(QStringLiteral("Half-lap extra"), m_halfLap);
    tabForm->addRow(QString(), m_apronHoles);
    tabForm->addRow(QStringLiteral("Hole from top"), m_holeFromTop);
    tabForm->addRow(QStringLiteral("Hole from bottom"), m_holeFromBottom);

    auto* ribs = addPage(sections, QStringLiteral("Ribs"), QStringLiteral("Where the long ribs and cross ribs sit."));
    auto* ribForm = formOf(ribs);
    m_crossSpacing = numSpin(1, 48, 12, 3, 1, QStringLiteral(" in"),
                             QStringLiteral("Requested spacing of cross ribs. They sit on hole midlines, the same distance from both ends. A leftover bay goes in the center."));
    m_longSpacing = numSpin(1, 48, 8, 3, 1, QStringLiteral(" in"),
                            QStringLiteral("Requested spacing of long ribs. They sit on hole midlines, the same distance from both edges. A leftover bay goes in the center."));
    ribForm->addRow(QStringLiteral("Cross rib spacing"), m_crossSpacing);
    ribForm->addRow(QStringLiteral("Long rib spacing"), m_longSpacing);

    auto* openings = addPage(sections, QStringLiteral("Openings"), QStringLiteral("Lightening openings in the rib bays."));
    auto* openForm = formOf(openings);
    m_lightening = check(QStringLiteral("Cut lightening openings"), true,
                         QStringLiteral("Rounded openings centered in each bay between ribs."));
    m_openingHeight = numSpin(0.5, 8, 2, 3, 0.25, QStringLiteral(" in"), QStringLiteral("Opening height. Radius is half of this."));
    m_openingMax = numSpin(1, 24, 6, 3, 0.5, QStringLiteral(" in"), QStringLiteral("Maximum length of one opening."));
    m_openingMin = numSpin(0.5, 12, 2, 3, 0.5, QStringLiteral(" in"), QStringLiteral("Skip a bay that cannot hold an opening this long."));
    m_openingGap = numSpin(0.25, 8, 2, 3, 0.25, QStringLiteral(" in"),
                           QStringLiteral("Solid land between repeated openings in a long bay."));
    m_openingSnap = numSpin(0, 1, 0.5, 3, 0.125, QStringLiteral(" in"),
                            QStringLiteral("Snap opening length down to this increment. Zero keeps the exact length."));
    m_landWide = numSpin(0.5, 8, 3, 3, 0.25, QStringLiteral(" in"),
                         QStringLiteral("Material left on each side of openings in a wide bay."));
    m_landNarrow = numSpin(0.25, 6, 1.75, 3, 0.25, QStringLiteral(" in"),
                           QStringLiteral("Material left on each side of openings in a narrow bay."));
    m_landThreshold = numSpin(4, 36, 10, 3, 1, QStringLiteral(" in"),
                              QStringLiteral("Bays at least this wide use the wide land."));
    openForm->addRow(QString(), m_lightening);
    openForm->addRow(QStringLiteral("Opening height"), m_openingHeight);
    openForm->addRow(QStringLiteral("Max length"), m_openingMax);
    openForm->addRow(QStringLiteral("Min length"), m_openingMin);
    openForm->addRow(QStringLiteral("Gap between"), m_openingGap);
    openForm->addRow(QStringLiteral("Snap"), m_openingSnap);
    openForm->addRow(QStringLiteral("Wide-bay land"), m_landWide);
    openForm->addRow(QStringLiteral("Narrow-bay land"), m_landNarrow);
    openForm->addRow(QStringLiteral("Wide-bay from"), m_landThreshold);

    auto* frame = addPage(sections, QStringLiteral("Frame"),
                          QStringLiteral("Legs weld inside the apron, up to the underside of the top. Stringers tie them near the floor."));
    auto* frameForm = formOf(frame);
    m_frame = check(QStringLiteral("Include legs"), true, QStringLiteral("Reference plan of the legs and stringers. It is not a cut file."));
    m_tube = numSpin(0.75, 8, 3, 3, 0.25, QStringLiteral(" in"), QStringLiteral("Leg tube outside size. Legs weld to the inside of the apron and run up to the underside of the top."));
    m_stringer = numSpin(0.5, 3, 2, 3, 0.125, QStringLiteral(" in"),
                         QStringLiteral("Stringer tube outside size. It cannot be larger than the leg."));
    m_stringerHeight = numSpin(0, 48, 4, 3, 0.25, QStringLiteral(" in"),
                               QStringLiteral("Height from the floor to the bottom of the stringers."));
    m_doubleStringers = check(QStringLiteral("Double long stringers"), false,
                              QStringLiteral("Front and back stringers, so a shelf can sit on the frame. One center stringer is enough without a shelf."));
    m_tubeWall = numSpin(0.06, 0.5, 0.1875, 4, 0.0625, QStringLiteral(" in"),
                         QStringLiteral("Wall thickness, used for the note. 0.1875 in is 3/16."));
    m_supports = new QSpinBox();
    m_supports->setRange(2, 12);
    m_supports->setValue(3);
    m_supports->setAlignment(Qt::AlignRight);
    m_supports->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_supports->setMinimumWidth(148);
    m_supports->setToolTip(QStringLiteral("Pairs of legs along the table, including both ends."));
    m_finished = numSpin(12, 60, 36, 3, 1, QStringLiteral(" in"),
                         QStringLiteral("Working-face height. The leg runs from the top of the foot plate to the underside of the top."));
    m_footNote = new QLabel();
    m_footNote->setWordWrap(true);
    connect(m_tube, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double leg) {
        m_stringer->setMaximum(leg);
    });
    m_legLength = new QLabel();
    m_legLength->setWordWrap(true);
    frameForm->addRow(QString(), m_frame);
    frameForm->addRow(QStringLiteral("Leg size"), m_tube);
    frameForm->addRow(QStringLiteral("Stringer size"), m_stringer);
    frameForm->addRow(QStringLiteral("Stringer height"), m_stringerHeight);
    frameForm->addRow(QString(), m_doubleStringers);
    frameForm->addRow(QStringLiteral("Tube wall"), m_tubeWall);
    frameForm->addRow(QStringLiteral("Leg pairs"), m_supports);
    frameForm->addRow(QStringLiteral("Finished height"), m_finished);
    frameForm->addRow(QStringLiteral("Foot plate"), m_footNote);
    frameForm->addRow(QStringLiteral("Leg example"), m_legLength);

    auto* nest = addPage(sections, QStringLiteral("Nest"), QStringLiteral("Sheet size and how parts are placed."));
    auto* nestForm = formOf(nest);
    m_sheetL = numSpin(12, 240, 120, 3, 1, QStringLiteral(" in"), QStringLiteral("Sheet length, the long axis of the nest."));
    m_sheetW = numSpin(12, 96, 60, 3, 1, QStringLiteral(" in"), QStringLiteral("Sheet width."));
    m_nestGap = numSpin(0, 2, 0.25, 3, 0.0625, QStringLiteral(" in"), QStringLiteral("Minimum gap between part bounding boxes."));
    m_nestMargin = numSpin(0, 2, 0.5, 3, 0.125, QStringLiteral(" in"), QStringLiteral("Keep-out margin inside the sheet edge."));
    m_nestCoupon = check(QStringLiteral("Include thin coupon on the nest"), true,
                         QStringLiteral("Place Q01 on the thin-stock sheet with the production parts."));
    m_q02 = check(QStringLiteral("Include top-stock coupon"), true, QStringLiteral("Q02 is a separate file, not nested on the thin sheet."));
    m_density = numSpin(0.2, 0.4, 0.2836, 4, 0.001, QStringLiteral(" lb/in3"), QStringLiteral("Steel density used for the weight estimate."));
    m_coupon = new QLabel();
    m_coupon->setWordWrap(true);
    nestForm->addRow(QStringLiteral("Sheet length"), m_sheetL);
    nestForm->addRow(QStringLiteral("Sheet width"), m_sheetW);
    nestForm->addRow(QStringLiteral("Part gap"), m_nestGap);
    nestForm->addRow(QStringLiteral("Edge margin"), m_nestMargin);
    nestForm->addRow(QString(), m_nestCoupon);
    nestForm->addRow(QString(), m_q02);
    nestForm->addRow(QStringLiteral("Density"), m_density);
    nestForm->addRow(QStringLiteral("Coupon slots"), m_coupon);

    auto* job = addPage(sections, QStringLiteral("Output"), QStringLiteral("Job title, shop notes, and which files to write."));
    auto* jobForm = formOf(job);
    m_title = new QLineEdit(QStringLiteral("Welding table"));
    m_title->setToolTip(QStringLiteral("Title printed on the PDF and in the README."));
    m_revision = new QLineEdit(QStringLiteral("A"));
    m_revision->setToolTip(QStringLiteral("Revision letter used in the PDF file name."));
    m_notes = new QPlainTextEdit();
    m_notes->setPlaceholderText(QStringLiteral("Optional shop notes for the last PDF page"));
    m_notes->setFixedHeight(64);
    m_writeParts = check(QStringLiteral("Individual DXF parts"), true, QStringLiteral("One DXF per part, origin shifted so coordinates are non-negative."));
    m_writeNest = check(QStringLiteral("Nest DXF"), true,
                        QStringLiteral("Thin-stock nest. Extra sheets are written when one sheet is not enough."));
    m_writeFrame = check(QStringLiteral("Leg plan DXF"), true, QStringLiteral("Reference plan of the legs and stringers. No cutting layers."));
    m_writePdf = check(QStringLiteral("Assembly PDF"), true, QStringLiteral("Multi-page fabrication packet."));
    m_writeReadme = check(QStringLiteral("README"), true, QStringLiteral("Cutting and assembly notes."));
    m_writeJson = check(QStringLiteral("Geometry JSON"), true, QStringLiteral("Audit numbers and a reloadable Job_Settings.json."));
    jobForm->addRow(QStringLiteral("Title"), m_title);
    jobForm->addRow(QStringLiteral("Revision"), m_revision);
    jobForm->addRow(QStringLiteral("Shop notes"), m_notes);
    jobForm->addRow(QString(), m_writeParts);
    jobForm->addRow(QString(), m_writeNest);
    jobForm->addRow(QString(), m_writeFrame);
    jobForm->addRow(QString(), m_writePdf);
    jobForm->addRow(QString(), m_writeReadme);
    jobForm->addRow(QString(), m_writeJson);
    dockLayout->addWidget(sectionHost, 1);

    m_hint = new QLabel(QStringLiteral("Hover or focus a field to see what it changes."));
    m_hint->setObjectName(QStringLiteral("hintLabel"));
    m_hint->setWordWrap(true);
    m_hint->setMinimumHeight(48);
    dockLayout->addWidget(m_hint);

    auto* outRow = new QHBoxLayout();
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString jobFolder = documents.isEmpty() ? QDir::home().filePath(QStringLiteral("Welding Table Generator"))
                                                  : QDir(documents).filePath(QStringLiteral("Welding Table Generator"));
    m_output = new QLineEdit(jobFolder);
    auto* browse = new QPushButton(QStringLiteral("Folder"));
    browse->setToolTip(QStringLiteral("Choose where the DXF, PDF, and notes are written."));
    outRow->addWidget(m_output, 1);
    outRow->addWidget(browse);
    dockLayout->addLayout(outRow);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Output folder"), m_output->text());
        if (!dir.isEmpty()) {
            m_output->setText(dir);
        }
    });

    m_openFolder = check(QStringLiteral("Open the folder after generating"), true, QStringLiteral("Open the output folder when the package is written."));
    dockLayout->addWidget(m_openFolder);
    auto* generateButton = new QPushButton(QStringLiteral("Generate package"));
    generateButton->setObjectName(QStringLiteral("generateButton"));
    generateButton->setToolTip(QStringLiteral("Write the DXF files, PDF, and notes. Ctrl+Enter."));
    dockLayout->addWidget(generateButton);
    connect(generateButton, &QPushButton::clicked, this, [this] { generate(); });
    auto* shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
    connect(shortcut, &QShortcut::activated, this, [this] { generate(); });

    dock->setWidget(dockBody);
    addDockWidget(Qt::LeftDockWidgetArea, dock);

    const QList<QDoubleSpinBox*> spins = {m_length,     m_width,         m_topThickness, m_webThickness, m_webDepth,
                                          m_holeMm,     m_pitchX,        m_pitchY,       m_marginX,      m_marginY,
                                          m_apronInset, m_clearance,     m_tabWidth,     m_slotExtra,    m_tabHeight,
                                          m_endTabLength, m_endTabHeight, m_ribGap,       m_halfLap,      m_holeFromTop,
                                          m_holeFromBottom, m_crossSpacing, m_longSpacing, m_openingHeight, m_openingMax,
                                          m_openingMin, m_openingGap,    m_openingSnap,  m_landWide,     m_landNarrow,
                                          m_landThreshold, m_tube,       m_stringer,     m_stringerHeight, m_tubeWall,
                                          m_finished,   m_sheetL,        m_sheetW,       m_nestGap,
                                          m_nestMargin, m_density};
    for (QDoubleSpinBox* spin : spins) {
        wire(spin);
    }
    wire(m_supports);
    for (QCheckBox* box : {m_square, m_apronTopSlots, m_apronHoles, m_lightening, m_frame, m_doubleStringers, m_nestCoupon, m_q02, m_writeParts, m_writeNest,
                           m_writeFrame, m_writePdf, m_writeReadme, m_writeJson}) {
        wire(box);
    }
    connect(m_title, &QLineEdit::textChanged, this, [this](const QString&) { onEdited(); });
    connect(m_revision, &QLineEdit::textChanged, this, [this](const QString&) { onEdited(); });
    connect(m_notes, &QPlainTextEdit::textChanged, this, [this] { onEdited(); });
    connect(m_holeUnit, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (m_loading) {
            return;
        }
        const double millimeters = index == 1 ? m_holeMm->value() : m_holeMm->value() * 25.4;
        m_loading = true;
        setHoleSpin(index == 1, millimeters);
        m_loading = false;
        onEdited();
    });
    connect(m_square, &QCheckBox::toggled, this, [this](bool locked) {
        m_pitchY->setEnabled(!locked);
        m_marginY->setEnabled(!locked);
    });
    m_pitchY->setEnabled(false);
    m_marginY->setEnabled(false);
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget* now) {
        if (now && !now->toolTip().isEmpty()) {
            m_hint->setText(now->toolTip());
        }
    });
}

void MainWindow::onEdited() {
    if (m_loading) {
        return;
    }
    if (m_square->isChecked()) {
        m_loading = true;
        m_pitchY->setValue(m_pitchX->value());
        m_marginY->setValue(m_marginX->value());
        m_loading = false;
    }
    if (m_preset->currentIndex() != 3) {
        m_loading = true;
        m_preset->setCurrentIndex(3);
        m_loading = false;
    }
    rebuild();
}

TableSpec MainWindow::collect() const {
    TableSpec spec;
    spec.length = m_length->value();
    spec.width = m_width->value();
    spec.topThickness = m_topThickness->value();
    spec.webThickness = m_webThickness->value();
    spec.webDepth = m_webDepth->value();
    spec.holeDiameterInch = m_holeUnit->currentIndex() == 1;
    spec.holeDiameterMm = spec.holeDiameterInch ? m_holeMm->value() * 25.4 : m_holeMm->value();
    spec.holePitchX = m_pitchX->value();
    spec.holePitchY = m_pitchY->value();
    spec.holeMarginX = m_marginX->value();
    spec.holeMarginY = m_marginY->value();
    spec.apronInset = m_apronInset->value();
    spec.apronTopSlots = m_apronTopSlots->isChecked();
    spec.slotClearance = m_clearance->value();
    spec.tabWidth = m_tabWidth->value();
    spec.slotExtra = m_slotExtra->value();
    spec.tabHeight = m_tabHeight->value();
    spec.endTabLength = m_endTabLength->value();
    spec.endTabHeight = m_endTabHeight->value();
    spec.ribEndGap = m_ribGap->value();
    spec.halfLapExtra = m_halfLap->value();
    spec.apronHoles = m_apronHoles->isChecked();
    spec.apronHoleFromTop = m_holeFromTop->value();
    spec.apronHoleFromBottom = m_holeFromBottom->value();
    spec.crossRibSpacing = m_crossSpacing->value();
    spec.longRibSpacing = m_longSpacing->value();
    spec.lightening = m_lightening->isChecked();
    spec.openingHeight = m_openingHeight->value();
    spec.openingMax = m_openingMax->value();
    spec.openingMin = m_openingMin->value();
    spec.openingGap = m_openingGap->value();
    spec.openingSnap = m_openingSnap->value();
    spec.landWide = m_landWide->value();
    spec.landNarrow = m_landNarrow->value();
    spec.landThreshold = m_landThreshold->value();
    spec.frame = m_frame->isChecked();
    spec.tubeSize = m_tube->value();
    spec.stringerSize = std::min(m_stringer->value(), m_tube->value());
    spec.stringerHeight = m_stringerHeight->value();
    spec.doubleStringers = m_doubleStringers->isChecked();
    spec.tubeWall = m_tubeWall->value();
    spec.frameSupports = m_supports->value();
    spec.finishedHeight = m_finished->value();
    spec.sheetLength = m_sheetL->value();
    spec.sheetWidth = m_sheetW->value();
    spec.nestGap = m_nestGap->value();
    spec.nestMargin = m_nestMargin->value();
    spec.nestCoupon = m_nestCoupon->isChecked();
    spec.includeQ02 = m_q02->isChecked();
    spec.density = m_density->value();
    spec.title = m_title->text().trimmed().isEmpty() ? QStringLiteral("Welding table") : m_title->text().trimmed();
    spec.revision = m_revision->text().trimmed().isEmpty() ? QStringLiteral("A") : m_revision->text().trimmed();
    spec.shopNotes = m_notes->toPlainText();
    spec.writeIndividuals = m_writeParts->isChecked();
    spec.writeNest = m_writeNest->isChecked();
    spec.writeFrame = m_writeFrame->isChecked();
    spec.writePdf = m_writePdf->isChecked();
    spec.writeReadme = m_writeReadme->isChecked();
    spec.writeJson = m_writeJson->isChecked();
    return spec;
}

void MainWindow::setHoleSpin(bool inches, double millimeters) {
    const QSignalBlocker unitBlock(m_holeUnit);
    const QSignalBlocker spinBlock(m_holeMm);
    m_holeUnit->setCurrentIndex(inches ? 1 : 0);
    if (inches) {
        m_holeMm->setDecimals(4);
        m_holeMm->setRange(0.125, 2.0);
        m_holeMm->setSingleStep(0.0625);
        m_holeMm->setSuffix(QStringLiteral(" in"));
        m_holeMm->setToolTip(QStringLiteral("Dog-hole diameter in inches. 5/8 in is 0.6250 in (15.875 mm)."));
        m_holeMm->setValue(millimeters / 25.4);
    } else {
        m_holeMm->setDecimals(3);
        m_holeMm->setRange(4, 50);
        m_holeMm->setSingleStep(0.5);
        m_holeMm->setSuffix(QStringLiteral(" mm"));
        m_holeMm->setToolTip(QStringLiteral("Dog-hole diameter in millimeters. 16 mm is 0.6299 in."));
        m_holeMm->setValue(millimeters);
    }
}

void MainWindow::applySpec(const TableSpec& spec) {
    m_length->setValue(spec.length);
    m_width->setValue(spec.width);
    m_topThickness->setValue(spec.topThickness);
    m_webThickness->setValue(spec.webThickness);
    m_webDepth->setValue(spec.webDepth);
    setHoleSpin(spec.holeDiameterInch, spec.holeDiameterMm);
    m_pitchX->setValue(spec.holePitchX);
    m_pitchY->setValue(spec.holePitchY);
    m_marginX->setValue(spec.holeMarginX);
    m_marginY->setValue(spec.holeMarginY);
    m_apronInset->setValue(spec.apronInset);
    m_apronTopSlots->setChecked(spec.apronTopSlots);
    m_clearance->setValue(spec.slotClearance);
    m_tabWidth->setValue(spec.tabWidth);
    m_slotExtra->setValue(spec.slotExtra);
    m_tabHeight->setValue(spec.tabHeight);
    m_endTabLength->setValue(spec.endTabLength);
    m_endTabHeight->setValue(spec.endTabHeight);
    m_ribGap->setValue(spec.ribEndGap);
    m_halfLap->setValue(spec.halfLapExtra);
    m_apronHoles->setChecked(spec.apronHoles);
    m_holeFromTop->setValue(spec.apronHoleFromTop);
    m_holeFromBottom->setValue(spec.apronHoleFromBottom);
    m_crossSpacing->setValue(spec.crossRibSpacing);
    m_longSpacing->setValue(spec.longRibSpacing);
    m_lightening->setChecked(spec.lightening);
    m_openingHeight->setValue(spec.openingHeight);
    m_openingMax->setValue(spec.openingMax);
    m_openingMin->setValue(spec.openingMin);
    m_openingGap->setValue(spec.openingGap);
    m_openingSnap->setValue(spec.openingSnap);
    m_landWide->setValue(spec.landWide);
    m_landNarrow->setValue(spec.landNarrow);
    m_landThreshold->setValue(spec.landThreshold);
    m_frame->setChecked(spec.frame);
    m_tube->setValue(spec.tubeSize);
    m_stringer->setMaximum(spec.tubeSize);
    m_stringer->setValue(std::min(spec.stringerSize, spec.tubeSize));
    m_stringerHeight->setValue(spec.stringerHeight);
    m_doubleStringers->setChecked(spec.doubleStringers);
    m_tubeWall->setValue(spec.tubeWall);
    m_supports->setValue(spec.frameSupports);
    m_finished->setValue(spec.finishedHeight);
    m_sheetL->setValue(spec.sheetLength);
    m_sheetW->setValue(spec.sheetWidth);
    m_nestGap->setValue(spec.nestGap);
    m_nestMargin->setValue(spec.nestMargin);
    m_nestCoupon->setChecked(spec.nestCoupon);
    m_q02->setChecked(spec.includeQ02);
    m_density->setValue(spec.density);
    m_title->setText(spec.title);
    m_revision->setText(spec.revision);
    m_notes->setPlainText(spec.shopNotes);
    m_writeParts->setChecked(spec.writeIndividuals);
    m_writeNest->setChecked(spec.writeNest);
    m_writeFrame->setChecked(spec.writeFrame);
    m_writePdf->setChecked(spec.writePdf);
    m_writeReadme->setChecked(spec.writeReadme);
    m_writeJson->setChecked(spec.writeJson);
}

void MainWindow::rebuild() {
    const bool built = m_model.rebuild(collect());
    m_preview->setModel(&m_model);
    m_vtk->setModel(&m_model);
    const bool bad = !built || !m_model.errors().isEmpty();
    m_summary->setText(m_model.summary());
    m_summary->setStyleSheet(bad ? QStringLiteral("color:#f0b4b4;") : QString());
    m_legLength->setText(QStringLiteral("%1 in").arg(QString::number(m_model.legLengthExample(), 'f', 3)));
    if (const Part* foot = m_model.find(QStringLiteral("F01"))) {
        m_footNote->setText(QStringLiteral("%1 in top stock, %2 in square, qty %3")
                                .arg(QString::number(foot->thickness, 'f', 3),
                                     QString::number(foot->bodyLength, 'f', 3))
                                .arg(foot->qty));
    } else {
        m_footNote->setText(QStringLiteral("Cut from the top thickness when legs are on."));
    }
    const double sw = m_model.slotWidth();
    m_coupon->setText(QStringLiteral("%1  %2  %3  %4  %5")
                          .arg(QString::number(sw - 0.004, 'f', 3), QString::number(sw, 'f', 3),
                               QString::number(sw + 0.004, 'f', 3), QString::number(sw + 0.008, 'f', 3),
                               QString::number(sw + 0.012, 'f', 3)));
    if (bad) {
        m_status->setText(m_model.errors().join(QStringLiteral(" ")));
    } else if (!m_model.warnings().isEmpty()) {
        m_status->setText(m_model.warnings().first());
    } else {
        m_status->setText(QStringLiteral("Ready."));
    }
}

void MainWindow::generate() {
    rebuild();
    if (!m_model.errors().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Check the settings"), m_model.errors().join(QStringLiteral("\n")));
        return;
    }
    m_status->setText(QStringLiteral("Writing the package..."));
    QApplication::processEvents();
    const PackageResult result = writePackage(m_model, m_output->text().trimmed());
    m_status->setText(result.message);
    if (!result.ok) {
        QMessageBox::warning(this, QStringLiteral("Generate"), result.message);
        return;
    }
    saveSettings();
    if (m_openFolder->isChecked() && !result.packageDir.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(result.packageDir));
    }
}

void MainWindow::loadSettings() {
    QSettings settings;
    if (!settings.contains(QStringLiteral("spec"))) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(settings.value(QStringLiteral("spec")).toByteArray());
    if (!doc.isObject()) {
        return;
    }
    m_loading = true;
    applySpec(TableSpec::fromJson(doc.object()));
    m_square->setChecked(settings.value(QStringLiteral("square"), true).toBool());
    m_pitchY->setEnabled(!m_square->isChecked());
    m_marginY->setEnabled(!m_square->isChecked());
    m_openFolder->setChecked(settings.value(QStringLiteral("openFolder"), true).toBool());
    const QString output = settings.value(QStringLiteral("output")).toString();
    const QString programFiles = QString::fromLocal8Bit(qgetenv("ProgramFiles"));
    const bool underProgramFiles = !programFiles.isEmpty() &&
        QDir::cleanPath(output).startsWith(QDir::cleanPath(programFiles), Qt::CaseInsensitive);
    const bool besideExe = QDir::cleanPath(output).startsWith(QDir::cleanPath(QCoreApplication::applicationDirPath()), Qt::CaseInsensitive);
    bool writable = false;
    if (!output.isEmpty() && !underProgramFiles) {
        QDir dir(output);
        writable = dir.mkpath(QStringLiteral("."));
        if (writable) {
            QFile probe(dir.filePath(QStringLiteral(".write-probe")));
            writable = probe.open(QIODevice::WriteOnly);
            if (writable) {
                probe.close();
                probe.remove();
            }
        }
    }
    if (!output.isEmpty() && !underProgramFiles && (writable || !besideExe)) {
        m_output->setText(output);
    }
    m_preset->setCurrentIndex(3);
    m_loading = false;
}

void MainWindow::saveSettings() const {
    QSettings settings;
    settings.setValue(QStringLiteral("spec"), QJsonDocument(collect().toJson()).toJson(QJsonDocument::Compact));
    settings.setValue(QStringLiteral("square"), m_square->isChecked());
    settings.setValue(QStringLiteral("openFolder"), m_openFolder->isChecked());
    settings.setValue(QStringLiteral("output"), m_output->text());
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveSettings();
    QMainWindow::closeEvent(event);
}
