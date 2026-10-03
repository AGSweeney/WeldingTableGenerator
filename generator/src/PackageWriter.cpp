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

#include "PackageWriter.h"

#include "DxfWriter.h"
#include "PaintTable.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>

#include <algorithm>
#include <cmath>

namespace {

QString inches(double v, int dec = 3) { return QString::number(v, 'f', dec); }

QString dimToken(double v) {
    if (std::abs(v - std::round(v)) < 1e-6) {
        return QString::number(static_cast<int>(std::lround(v)));
    }
    QString s = QString::number(v, 'f', 3);
    while (s.contains(QLatin1Char('.')) && (s.endsWith(QLatin1Char('0')) || s.endsWith(QLatin1Char('.')))) {
        s.chop(1);
    }
    return s;
}

QString partFileName(const Part& part) {
    QString name = part.name;
    name.replace(QLatin1Char(' '), QLatin1Char('_'));
    return QStringLiteral("%1_%2_%3in_QTY%4.dxf").arg(part.code, name, inches(part.thickness), QString::number(part.qty));
}

void writeTextFile(const QString& path, const QString& text, QStringList& files, QString& error) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        error = QStringLiteral("Could not write %1").arg(path);
        return;
    }
    file.write(text.toUtf8());
    files << path;
}

QString listOf(const std::vector<double>& values) {
    QStringList parts;
    for (double v : values) {
        parts << inches(v, 3);
    }
    if (parts.isEmpty()) {
        return QStringLiteral("(none)");
    }
    return parts.join(QStringLiteral(", "));
}

QString readmeFor(const TableModel& model) {
    const TableSpec& s = model.spec();
    const QString date = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
    QString ribs;
    ribs += QStringLiteral("- Top: %1 x %2 x %3 in.\n").arg(inches(s.length), inches(s.width), inches(s.topThickness));
    ribs += QStringLiteral("- Dog holes: %1, %2 x %3 in grid, margins %4 / %5 in. %6 holes.\n")
                .arg(model.dogHoleLabel(), inches(s.holePitchX), inches(s.holePitchY), inches(s.holeMarginX),
                     inches(s.holeMarginY))
                .arg(model.topHoleCount());
    ribs += QStringLiteral("- Webs: %1 in stock, %2 in deep. %3 long ribs, %4 cross ribs.\n")
                .arg(inches(s.webThickness), inches(s.webDepth))
                .arg(model.longRibCount())
                .arg(model.crossRibCount());
    ribs += QStringLiteral("- Slots are %1 wide (stock plus %2 clearance) and %3 long over a %4 tab.\n")
                .arg(inches(model.slotWidth()), inches(s.slotClearance), inches(model.topSlotLength()), inches(s.tabWidth));
    QString files;
    for (const Part& p : model.parts()) {
        if (p.qty < 1) {
            continue;
        }
        files += QStringLiteral("- %1: %2; qty %3; %4 in.\n").arg(p.code, p.name).arg(p.qty).arg(inches(p.thickness));
    }
    QString nest = !model.nestOk()
                       ? QStringLiteral("The thin-stock nest does not fit on the selected sheet. %1\n").arg(model.nestError())
                   : model.nestSheetCount() <= 1
                       ? QStringLiteral("The thin-stock nest fits on one %1 x %2 sheet. Occupied span is about X %3 to %4, Y %5 to %6.\n")
                             .arg(inches(s.sheetLength, 1), inches(s.sheetWidth, 1), inches(model.nestOccupied().left()),
                                  inches(model.nestOccupied().right()), inches(model.nestOccupied().top()),
                                  inches(model.nestOccupied().bottom()))
                       : QStringLiteral("The thin-stock nest uses %1 sheets of %2 x %3. Each sheet is a separate DXF and PDF page.\n")
                             .arg(model.nestSheetCount())
                             .arg(inches(s.sheetLength, 1), inches(s.sheetWidth, 1));
    return QStringLiteral(
               "# %1 %2 x %3 - Revision %4\n\n"
               "Laser-cut fabrication layout generated %5. Dimensions are inches unless stated otherwise.\n\n"
               "## Selections\n%6\n"
               "## Files\n%7"
               "- N01, N02, ...: one thin-stock nest sheet each. Use the nest or the individual thin parts, not both.\n"
               "- R01: leg and stringer plan, reference only, when legs are enabled.\n"
               "- F01 foot plates are cut from the top thickness. The tube nest is on the frame sheet, not a DXF.\n"
               "- Assembly PDF, Geometry_Checks.json, and Job_Settings.json.\n\n"
               "## CAM\n"
               "DXF AC1018, full size, coordinates in inches. Cut only CUT_OUTER and CUT_INNER. "
               "REF_NO_CUT and LABEL_NO_CUT must stay off. Geometry is nominal finished size. Apply kerf in CAM.\n\n"
               "Test the coupons before cutting the plates. Coupon slot widths, left to right, are the design slot "
               "width plus -0.004, 0, +0.004, +0.008, and +0.012 in. Q01 carries the sample tab.\n\n"
               "## Sheet and weight\n%8"
               "Net plate-and-web mass is about %9 lb at %10 lb/in3, excluding tube, welds, feet, and coupons. "
               "Lightening openings remove about %11 lb.\n\n"
               "## Assembly\n"
               "1. Cut coupons and approve tab fit and dog-hole fit.\n"
               "2. Deburr without rounding the top-bearing shoulders.\n"
               "3. Assemble the rib cage off the top. Half-laps on the long ribs open downward; half-laps on the cross ribs open upward toward the top.\n"
               "4. Slide the end aprons onto the long-rib end tabs, then the long aprons onto the cross-rib end tabs.\n"
               "5. Dry-fit every top tab into the top before welding the cage solid.\n"
               "6. Weld with the working face down on a flat surface. Tack from the center outward.\n"
               "7. Weld the legs to the inside of the apron, up to the underside of the top. Foot plates are cut from the top thickness. Use one center stringer, or two long stringers if the frame will carry a shelf. Saw the tube nest from 20 ft or 24 ft stock.\n\n"
               "Example leg length at %12 in finished height with a %13 in foot plate: %14 in "
               "(finished height minus the top and the foot plate).\n")
        .arg(s.title)
        .arg(dimToken(s.width))
        .arg(dimToken(s.length))
        .arg(s.revision)
        .arg(date)
        .arg(ribs)
        .arg(files)
        .arg(nest)
        .arg(inches(model.assemblyWeight(), 1))
        .arg(inches(s.density, 4))
        .arg(inches(model.lighteningSaved(), 1))
        .arg(inches(s.finishedHeight, 3))
        .arg(inches(s.topThickness, 3))
        .arg(inches(model.legLengthExample(), 3));
}

void writePdf(const TableModel& model, const QString& path, QStringList& files, QString& error) {
    QPdfWriter pdf(path);
    pdf.setTitle(model.spec().title);
    pdf.setCreator(QStringLiteral("Welding Table Generator"));
    pdf.setResolution(72);
    pdf.setPageSize(QPageSize(QSizeF(17, 11), QPageSize::Inch));
    pdf.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter painter(&pdf);
    if (!painter.isActive()) {
        error = QStringLiteral("Could not open the PDF writer.");
        return;
    }
    const TableSpec& s = model.spec();
    const int W = pdf.width();
    const int H = pdf.height();
    const int nestPages = std::max(1, model.nestOk() ? model.nestSheetCount() : 1);
    const int pageCount = 4 + nestPages + (s.frame ? 1 : 0);
    int page = 1;
    const QColor navy(24, 50, 68);
    const QColor teal(22, 133, 141);
    const QColor gray(104, 121, 133);
    const QString date = QDate::currentDate().toString(QStringLiteral("dd MMM yyyy"));

    auto header = [&](const QString& title) {
        painter.fillRect(QRectF(0, 0, W, 72), navy);
        painter.setPen(Qt::white);
        QFont bold(QStringLiteral("Segoe UI"), 20, QFont::Bold);
        painter.setFont(bold);
        painter.drawText(QRectF(36, 12, W - 72, 32), Qt::AlignVCenter, title);
        QFont sub(QStringLiteral("Segoe UI"), 10);
        painter.setFont(sub);
        painter.drawText(QRectF(36, 42, W - 72, 20), Qt::AlignVCenter,
                         QStringLiteral("%1 x %2 in  |  %3 in top  |  %4 in webs  |  Rev %5  |  %6")
                             .arg(dimToken(s.width), dimToken(s.length), inches(s.topThickness), inches(s.webThickness),
                                  s.revision, date));
    };
    auto footer = [&]() {
        painter.setPen(gray);
        painter.setFont(QFont(QStringLiteral("Segoe UI"), 9));
        painter.drawText(QRectF(36, H - 32, W - 72, 20), Qt::AlignVCenter,
                         QStringLiteral("Dimensions in inches. Drawings are not to scale. Cut files are full size."));
        painter.drawText(QRectF(36, H - 32, W - 72, 20), Qt::AlignRight | Qt::AlignVCenter,
                         QStringLiteral("%1 / %2").arg(page).arg(pageCount));
    };
    auto bodyText = [&](int x, int y, const QStringList& lines, int size = 11) {
        painter.setPen(navy);
        painter.setFont(QFont(QStringLiteral("Segoe UI"), size));
        int yy = y;
        for (const QString& line : lines) {
            painter.drawText(QRectF(x, yy, W - x - 36, 22), Qt::AlignVCenter, line);
            yy += size + 8;
        }
    };

    header(s.title);
    paintTable(painter, QRectF(28, 84, W - 56, 380), model, TableView::Iso, false, 1, {});
    bodyText(40, 478,
             {QStringLiteral("Assembled table. Drag the 3D view to spin it. Legs weld inside the apron, with stringers near the floor."),
              QStringLiteral("Overall top %1 x %2 x %3. Finished plate depth %4.")
                  .arg(inches(s.length), inches(s.width), inches(s.topThickness), inches(s.topThickness + s.webDepth)),
              QStringLiteral("%1 dog holes, %2, on a %3 x %4 in grid.")
                  .arg(model.topHoleCount())
                  .arg(model.dogHoleLabel())
                  .arg(inches(s.holePitchX))
                  .arg(inches(s.holePitchY)),
              QStringLiteral("%1 long ribs and %2 cross ribs, %3 deep, %4 stock.")
                  .arg(model.longRibCount())
                  .arg(model.crossRibCount())
                  .arg(inches(s.webDepth), inches(s.webThickness)),
              QStringLiteral("Plate and web steel about %1 lb. Lightening removes about %2 lb.")
                  .arg(inches(model.assemblyWeight(), 1), inches(model.lighteningSaved(), 1)),
              QStringLiteral("Top tabs are %1 tall, leaving %2 recess at the working face.")
                  .arg(inches(s.tabHeight), inches(s.topThickness - s.tabHeight)),
              QStringLiteral("Slot width %1. Extra slot length %2. Rib end gap %3.")
                  .arg(inches(model.slotWidth()), inches(s.slotExtra), inches(s.ribEndGap))});
    footer();

    pdf.newPage();
    ++page;
    header(QStringLiteral("Top and assembly coordinates"));
    paintTable(painter, QRectF(28, 84, W - 56, 340), model, TableView::Plan, false, 1, {});
    QStringList coords;
    coords << QStringLiteral("Datum: X from the left end, Y from the front edge. Web depth is down from the top underside.");
    coords << QStringLiteral("Hole X: %1").arg(listOf(model.holeX()));
    coords << QStringLiteral("Hole Y: %1").arg(listOf(model.holeY()));
    coords << QStringLiteral("Top-slot X: %1").arg(listOf(model.tabX()));
    coords << QStringLiteral("Top-slot Y: %1").arg(listOf(model.tabY()));
    coords << QStringLiteral("Cross rib X: %1").arg(listOf(model.crossX()));
    coords << QStringLiteral("Long rib Y: %1").arg(listOf(model.longY()));
    coords << QStringLiteral("Long aprons run X %1 to %2. End aprons run Y %3 to %4.")
                  .arg(inches(s.apronInset), inches(s.length - s.apronInset), inches(model.apronInner()),
                       inches(s.width - model.apronInner()));
    bodyText(40, 440, coords, 10);
    footer();

    pdf.newPage();
    ++page;
    header(QStringLiteral("Cut parts"));
    paintTable(painter, QRectF(28, 80, W - 56, 430), model, TableView::Profiles, false, 1, {});
    bodyText(40, 530,
             {QStringLiteral("Openings are centered in each bay, %1 high, snapped down to %2 in.")
                  .arg(inches(s.openingHeight), inches(s.openingSnap)),
              QStringLiteral("Wide bays (at least %1 in) keep %2 in of land each side. Narrow bays keep %3 in.")
                  .arg(inches(s.landThreshold, 1), inches(s.landWide), inches(s.landNarrow)),
              QStringLiteral("Half-laps pass mid-depth by %1 in on each rib, %2 in total clearance.")
                  .arg(inches(s.halfLapExtra), inches(s.halfLapExtra * 2)),
              QStringLiteral("End tabs are %1 long and %2 high, centered on the web.")
                  .arg(inches(s.endTabLength), inches(s.endTabHeight)),
              s.lightening ? QStringLiteral("Lightening is enabled.") : QStringLiteral("Lightening openings are off.")});
    footer();

    for (int sheet = 0; sheet < nestPages; ++sheet) {
        pdf.newPage();
        ++page;
        header(nestPages > 1 ? QStringLiteral("Thin-stock nest  %1 / %2").arg(sheet + 1).arg(nestPages)
                             : QStringLiteral("Thin-stock nest"));
        paintTable(painter, QRectF(28, 80, W - 56, 520), model, TableView::Nest, false, 1, {}, kOrbitYaw, kOrbitPitch,
                   model.nestOk() ? sheet : -1);
        const int onSheet = [&] {
            int n = 0;
            for (const Placement& pl : model.nest()) {
                if (pl.sheet == sheet) {
                    ++n;
                }
            }
            return n;
        }();
        bodyText(40, 620,
                 {model.nestOk() ? (nestPages > 1 ? QStringLiteral("Sheet %1 of %2. %3 x %4 in. Part gap %5 in. Edge margin %6 in. %7 parts.")
                                                        .arg(sheet + 1)
                                                        .arg(nestPages)
                                                        .arg(inches(s.sheetLength, 1))
                                                        .arg(inches(s.sheetWidth, 1))
                                                        .arg(inches(s.nestGap))
                                                        .arg(inches(s.nestMargin))
                                                        .arg(onSheet)
                                                  : QStringLiteral("One %1 x %2 sheet. Part gap %3 in. Edge margin %4 in.")
                                                        .arg(inches(s.sheetLength, 1), inches(s.sheetWidth, 1), inches(s.nestGap),
                                                             inches(s.nestMargin)))
                                 : model.nestError(),
                  QStringLiteral("Cut inner features before the outside profile. Q02 is separate top-stock scrap and is not in this nest.")});
        footer();
    }

    if (s.frame) {
        pdf.newPage();
        ++page;
        header(QStringLiteral("Legs and stringers"));
        paintTable(painter, QRectF(28, 84, W - 56, 340), model, TableView::Frame, false, 1, {});
        QStringList frameLines = {
            QStringLiteral("Reference plan only. Legs %1 in square, stringers %2 in, wall %3 in.")
                .arg(inches(s.tubeSize), inches(s.stringerSize), inches(s.tubeWall, 4)),
            QStringLiteral("Legs weld to the inside of the apron, up to the underside of the top. Intermediate legs sit in a corner between a cross rib and the apron."),
            s.doubleStringers
                ? QStringLiteral("%1 leg pairs. Two long stringers, %2 in each, for a shelf. Cross stringers %3 in.")
                      .arg(s.frameSupports)
                      .arg(inches(model.railLength()))
                      .arg(inches(model.crossmemberLength()))
                : QStringLiteral("%1 leg pairs. Center stringer is cut between the cross tubes. Cross stringers %2 in.")
                      .arg(s.frameSupports)
                      .arg(inches(model.crossmemberLength())),
            QStringLiteral("Foot plates are %1 in square, %2 in thick, cut from the top stock. Qty %3.")
                .arg(inches(model.footPlateSize()), inches(model.footPlateThickness()))
                .arg(s.frameSupports * 2),
            QStringLiteral("Leg length %1 in. Stringer bottom is %2 in off the floor.")
                .arg(inches(model.legLengthExample()), inches(s.stringerHeight))};
        for (const TubeNest& nest : model.tubeNests()) {
            frameLines << QStringLiteral("%1 in tube nests on %2 ft stock, %3 stick(s), kerf %4 in.")
                              .arg(inches(nest.size), QString::number(nest.stockFeet, 'f', 0))
                              .arg(nest.sticks.size())
                              .arg(inches(kTubeKerf));
        }
        bodyText(40, 436, frameLines, 10);
        footer();
    }

    pdf.newPage();
    ++page;
    header(QStringLiteral("Bill of materials and fit-up"));
    painter.setPen(navy);
    painter.setFont(QFont(QStringLiteral("Segoe UI"), 11, QFont::Bold));
    const int cols[] = {40, 120, 340, 420, 560, 820};
    const QStringList head = {QStringLiteral("Part"), QStringLiteral("Description"), QStringLiteral("Qty"),
                              QStringLiteral("Stock"), QStringLiteral("Body size"), QStringLiteral("lb each")};
    for (int i = 0; i < head.size(); ++i) {
        painter.drawText(cols[i], 110, head[i]);
    }
    painter.setFont(QFont(QStringLiteral("Segoe UI"), 11));
    int row = 0;
    for (const Part& part : model.parts()) {
        if (!part.code.startsWith(QLatin1Char('P'))) {
            continue;
        }
        const int y = 136 + row * 22;
        const QString body = part.code == QLatin1String("P01")
                                 ? QStringLiteral("%1 x %2").arg(inches(s.length), inches(s.width))
                                 : QStringLiteral("%1 x %2").arg(inches(part.bodyLength), inches(s.webDepth));
        const QStringList vals = {part.code, part.name, QString::number(part.qty), inches(part.thickness), body,
                                  inches(part.weight, 1)};
        for (int i = 0; i < vals.size(); ++i) {
            painter.drawText(cols[i], y, vals[i]);
        }
        ++row;
    }
    QStringList steps = {
        QStringLiteral("Cut Q01 before the plates. Fit its tab through the coupon slots and check a real dog against the hole."),
        QStringLiteral("Import DXF at 1:1 inches. Confirm the top is %1 x %2 and a hole is %3 in.")
            .arg(inches(s.length), inches(s.width), inches(model.holeDiameterIn(), 6)),
        QStringLiteral("Deburr, assemble the cage, then dry-fit all %1 top tabs before final welds.").arg(model.topSlotCount()),
        QStringLiteral("No load rating, weld schedule, or flatness certification is supplied with this layout.")};
    if (!s.shopNotes.trimmed().isEmpty()) {
        steps << s.shopNotes.trimmed();
    }
    bodyText(40, 136 + row * 22 + 24, steps, 11);
    footer();

    painter.end();
    files << path;
}

}  // namespace

PackageResult writePackage(const TableModel& model, const QString& outputDir) {
    PackageResult result;
    if (!model.errors().isEmpty() || model.parts().empty()) {
        result.message = model.errors().isEmpty() ? QStringLiteral("Nothing to write.") : model.errors().join(QStringLiteral("\n"));
        return result;
    }
    QDir root(outputDir);
    if (outputDir.trimmed().isEmpty() || !root.mkpath(QStringLiteral("."))) {
        result.message = QStringLiteral("Choose a folder the program can write to.");
        return result;
    }
    const TableSpec& spec = model.spec();
    QString error;
    if (spec.writeIndividuals || spec.writeNest || spec.writeFrame) {
        root.mkpath(QStringLiteral("DXF"));
        QDir dxf(root.filePath(QStringLiteral("DXF")));
        const QStringList old = dxf.entryList({QStringLiteral("*.dxf")}, QDir::Files);
        for (const QString& name : old) {
            dxf.remove(name);
        }
        if (spec.writeIndividuals) {
            for (const Part& part : model.parts()) {
                if (part.qty < 1) {
                    continue;
                }
                const QString path = dxf.filePath(partFileName(part));
                if (!writePartDxf(path, part, error)) {
                    result.message = error;
                    return result;
                }
                result.files << path;
            }
        }
        if (spec.writeNest) {
            if (!model.nestOk()) {
                result.message = model.nestError();
            } else {
                for (int sheet = 0; sheet < model.nestSheetCount(); ++sheet) {
                    const QString path = dxf.filePath(QStringLiteral("N%1_%2in_%3x%4_Sheet_Nest.dxf")
                                                         .arg(sheet + 1, 2, 10, QChar('0'))
                                                         .arg(inches(spec.webThickness), dimToken(spec.sheetWidth),
                                                              dimToken(spec.sheetLength)));
                    if (!writeNestDxf(path, model, sheet, error)) {
                        result.message = error;
                        return result;
                    }
                    result.files << path;
                }
            }
        }
        if (spec.writeFrame && spec.frame && !model.frame().empty()) {
            const QString path = dxf.filePath(QStringLiteral("R01_Tube_Frame_Plan_REFERENCE_ONLY.dxf"));
            if (!writeFrameDxf(path, model, error)) {
                result.message = error;
                return result;
            }
            result.files << path;
        }
    }
    if (!error.isEmpty()) {
        result.message = error;
        return result;
    }
    if (spec.writePdf) {
        const QString path = root.filePath(QStringLiteral("Welding_Table_%1x%2_Assembly_Rev%3.pdf")
                                               .arg(dimToken(spec.width), dimToken(spec.length), spec.revision));
        writePdf(model, path, result.files, error);
        if (!error.isEmpty()) {
            result.message = error;
            return result;
        }
    }
    if (spec.writeReadme) {
        writeTextFile(root.filePath(QStringLiteral("README_Cutting_and_Assembly.md")), readmeFor(model), result.files, error);
        if (!error.isEmpty()) {
            result.message = error;
            return result;
        }
    }
    if (spec.writeJson) {
        QJsonObject report = spec.toJson();
        report.insert(QStringLiteral("top_holes"), model.topHoleCount());
        report.insert(QStringLiteral("top_slots"), model.topSlotCount());
        report.insert(QStringLiteral("long_stiffeners"), model.longRibCount());
        report.insert(QStringLiteral("cross_stiffeners"), model.crossRibCount());
        report.insert(QStringLiteral("nest_ok"), model.nestOk());
        report.insert(QStringLiteral("nest_sheets"), model.nestSheetCount());
        report.insert(QStringLiteral("assembly_weight_lb"), model.assemblyWeight());
        report.insert(QStringLiteral("lightening_weight_saved_lb"), model.lighteningSaved());
        report.insert(QStringLiteral("hole_diameter_in"), model.holeDiameterIn());
        QJsonArray checks;
        for (const QString& c : model.checks()) {
            checks.append(c);
        }
        report.insert(QStringLiteral("checks"), checks);
        QJsonArray warnings;
        for (const QString& w : model.warnings()) {
            warnings.append(w);
        }
        report.insert(QStringLiteral("warnings"), warnings);
        QJsonArray parts;
        for (const Part& part : model.parts()) {
            QJsonObject item;
            item.insert(QStringLiteral("id"), part.code);
            item.insert(QStringLiteral("description"), part.name);
            item.insert(QStringLiteral("qty"), part.qty);
            item.insert(QStringLiteral("thickness_in"), part.thickness);
            item.insert(QStringLiteral("each_lb"), part.weight);
            item.insert(QStringLiteral("body_length_in"), part.bodyLength);
            parts.append(item);
        }
        report.insert(QStringLiteral("parts"), parts);
        writeTextFile(root.filePath(QStringLiteral("Geometry_Checks.json")),
                      QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented)), result.files, error);
        writeTextFile(root.filePath(QStringLiteral("Job_Settings.json")),
                      QString::fromUtf8(QJsonDocument(spec.toJson()).toJson(QJsonDocument::Indented)), result.files, error);
        if (!error.isEmpty()) {
            result.message = error;
            return result;
        }
    }
    result.ok = true;
    result.message = QStringLiteral("Wrote %1 files to %2").arg(result.files.size()).arg(root.absolutePath());
    if (!model.warnings().isEmpty()) {
        result.message += QStringLiteral("\n%1").arg(model.warnings().join(QStringLiteral("\n")));
    }
    return result;
}
