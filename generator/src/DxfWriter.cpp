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

#include "DxfWriter.h"

#include "drw_interface.h"
#include "libdxfrw.h"

#include <QFile>
#include <QFileInfo>
#include <QStringList>

#include <algorithm>
#include <cmath>

class AcadWriter : public DRW_Interface {
public:
    AcadWriter(const std::vector<DxfWriter::Item>& items, dxfRW* writer)
        : m_items(items), m_writer(writer) {}

    void writeHeader(DRW_Header& data) override {
        data.addStr("$ACADVER", std::string("AC1018"), 1);
        data.addInt("$INSUNITS", 1, 70);
        data.addInt("$MEASUREMENT", 0, 70);
    }

    void writeLayers() override {
        const struct {
            const char* name;
            int color;
        } layers[] = {{"0", 7}, {"CUT_OUTER", 7}, {"CUT_INNER", 3}, {"REF_NO_CUT", 8}, {"LABEL_NO_CUT", 2}};
        for (const auto& layer : layers) {
            DRW_Layer record;
            record.name = layer.name;
            record.color = layer.color;
            m_writer->writeLayer(&record);
        }
    }

    void writeEntities() override {
        for (const DxfWriter::Item& item : m_items) {
            if (item.kind == 1) {
                DRW_Circle circle;
                circle.layer = item.layer.toStdString();
                circle.basePoint.x = item.x;
                circle.basePoint.y = item.y;
                circle.radious = item.r;
                m_writer->writeCircle(&circle);
                continue;
            }
            if (item.kind == 2) {
                DRW_Text text;
                text.layer = item.layer.toStdString();
                text.basePoint.x = item.x;
                text.basePoint.y = item.y;
                text.height = item.h;
                text.text = item.text.toStdString();
                m_writer->writeText(&text);
                continue;
            }
            DRW_LWPolyline poly;
            poly.layer = item.layer.toStdString();
            poly.flags = 1;
            for (size_t i = 0; i < item.pts.size(); ++i) {
                const double bulge = i < item.bulges.size() ? item.bulges[i] : 0.0;
                poly.addVertex(DRW_Vertex2D(item.pts[i].x(), item.pts[i].y(), bulge));
            }
            m_writer->writeLWPolyline(&poly);
        }
    }

    void writeBlocks() override {}
    void writeBlockRecords() override {}
    void writeLTypes() override {}
    void writeTextstyles() override {}
    void writeVports() override {}
    void writeDimstyles() override {}
    void writeObjects() override {}
    void writeAppId() override {}
    void addHeader(const DRW_Header*) override {}
    void addLType(const DRW_LType&) override {}
    void addLayer(const DRW_Layer&) override {}
    void addDimStyle(const DRW_Dimstyle&) override {}
    void addVport(const DRW_Vport&) override {}
    void addTextStyle(const DRW_Textstyle&) override {}
    void addAppId(const DRW_AppId&) override {}
    void addBlock(const DRW_Block&) override {}
    void setBlock(const int) override {}
    void endBlock() override {}
    void addPoint(const DRW_Point&) override {}
    void addLine(const DRW_Line&) override {}
    void addRay(const DRW_Ray&) override {}
    void addXline(const DRW_Xline&) override {}
    void addArc(const DRW_Arc&) override {}
    void addCircle(const DRW_Circle&) override {}
    void addEllipse(const DRW_Ellipse&) override {}
    void addSpline(const DRW_Spline*) override {}
    void addKnot(const DRW_Entity&) override {}
    void addInsert(const DRW_Insert&) override {}
    void addTrace(const DRW_Trace&) override {}
    void add3dFace(const DRW_3Dface&) override {}
    void addSolid(const DRW_Solid&) override {}
    void addMText(const DRW_MText&) override {}
    void addText(const DRW_Text&) override {}
    void addDimAlign(const DRW_DimAligned*) override {}
    void addDimLinear(const DRW_DimLinear*) override {}
    void addDimRadial(const DRW_DimRadial*) override {}
    void addDimDiametric(const DRW_DimDiametric*) override {}
    void addDimAngular(const DRW_DimAngular*) override {}
    void addDimAngular3P(const DRW_DimAngular3p*) override {}
    void addDimOrdinate(const DRW_DimOrdinate*) override {}
    void addLeader(const DRW_Leader*) override {}
    void addHatch(const DRW_Hatch*) override {}
    void addViewport(const DRW_Viewport&) override {}
    void addImage(const DRW_Image*) override {}
    void linkImage(const DRW_ImageDef*) override {}
    void addComment(const char*) override {}
    void addLWPolyline(const DRW_LWPolyline&) override {}
    void addPolyline(const DRW_Polyline&) override {}
    void addPlotSettings(const DRW_PlotSettings*) override {}

private:
    const std::vector<DxfWriter::Item>& m_items;
    dxfRW* m_writer;
};

void DxfWriter::addPolyline(const std::vector<QPointF>& pts, const QString& layer, double ox, double oy) {
    if (pts.size() < 3) {
        return;
    }
    Item item;
    item.layer = layer;
    item.pts.reserve(pts.size());
    for (const QPointF& p : pts) {
        item.pts.push_back(QPointF(p.x() + ox, p.y() + oy));
    }
    m_items.push_back(std::move(item));
}

void DxfWriter::addBulgePolyline(const std::vector<QPointF>& pts, const std::vector<double>& bulges, const QString& layer) {
    if (pts.size() < 2 || pts.size() != bulges.size()) {
        return;
    }
    Item item;
    item.layer = layer;
    item.pts = pts;
    item.bulges = bulges;
    m_items.push_back(std::move(item));
}

void DxfWriter::addCircle(double x, double y, double r, const QString& layer) {
    Item item;
    item.kind = 1;
    item.layer = layer;
    item.x = x;
    item.y = y;
    item.r = r;
    m_items.push_back(std::move(item));
}

void DxfWriter::addText(double x, double y, double height, const QString& layer, const QString& text) {
    Item item;
    item.kind = 2;
    item.layer = layer;
    item.x = x;
    item.y = y;
    item.h = height;
    item.text = text;
    m_items.push_back(std::move(item));
}

bool DxfWriter::save(const QString& path, QString& error) const {
    const QByteArray encoded = QFile::encodeName(path);
    dxfRW writer(encoded.constData());
    AcadWriter iface(m_items, &writer);
    if (!writer.write(&iface, DRW::AC1018, false)) {
        error = QStringLiteral("Could not write %1").arg(path);
        return false;
    }
    if (QFileInfo(path).size() < 64) {
        error = QStringLiteral("Could not write %1").arg(path);
        return false;
    }
    return true;
}

namespace {

void addCapsule(DxfWriter& dxf, const CapsuleFeat& cap, double ox, double oy) {
    const double r = cap.height * 0.5;
    if (cap.length <= cap.height + 1e-6) {
        dxf.addCircle(cap.x + ox, cap.y + oy, r, QStringLiteral("CUT_INNER"));
        return;
    }
    const double a = cap.x - cap.length * 0.5 + r + ox;
    const double b = cap.x + cap.length * 0.5 - r + ox;
    const double y = cap.y + oy;
    dxf.addBulgePolyline({QPointF(a, y - r), QPointF(b, y - r), QPointF(b, y + r), QPointF(a, y + r)},
                         {0, 1, 0, 1}, QStringLiteral("CUT_INNER"));
}

void addPartGeometry(DxfWriter& dxf, const Part& part, double ox, double oy, bool cut) {
    const QString outer = cut ? QStringLiteral("CUT_OUTER") : QStringLiteral("REF_NO_CUT");
    const QString inner = cut ? QStringLiteral("CUT_INNER") : QStringLiteral("REF_NO_CUT");
    dxf.addPolyline(part.outer, outer, ox, oy);
    for (const CircleFeat& h : part.holes) {
        dxf.addCircle(h.x + ox, h.y + oy, h.r, inner);
    }
    for (const SlotFeat& s : part.slotCuts) {
        dxf.addPolyline({QPointF(s.x0, s.y0), QPointF(s.x1, s.y0), QPointF(s.x1, s.y1), QPointF(s.x0, s.y1)}, inner, ox,
                        oy);
    }
    for (const CapsuleFeat& c : part.caps) {
        addCapsule(dxf, c, ox, oy);
    }
}

}  // namespace

bool writePartDxf(const QString& path, const Part& part, QString& error) {
    DxfWriter dxf;
    const double ox = -part.bounds.left();
    const double oy = -part.bounds.top();
    addPartGeometry(dxf, part, ox, oy, true);
    return dxf.save(path, error);
}

bool writeNestDxf(const QString& path, const TableModel& model, int sheet, QString& error) {
    DxfWriter dxf;
    const double L = model.spec().sheetLength;
    const double W = model.spec().sheetWidth;
    dxf.addPolyline({QPointF(0, 0), QPointF(L, 0), QPointF(L, W), QPointF(0, W)}, QStringLiteral("REF_NO_CUT"));
    for (const Placement& pl : model.nest()) {
        if (pl.sheet != sheet) {
            continue;
        }
        const Part& part = model.parts().at(static_cast<size_t>(pl.partIndex));
        addPartGeometry(dxf, part, pl.dx, pl.dy, true);
        const QRectF b = part.bounds.translated(pl.dx, pl.dy);
        dxf.addText(b.left() + 0.4, b.center().y(), 0.28, QStringLiteral("LABEL_NO_CUT"), pl.label);
    }
    const int sheets = std::max(1, model.nestSheetCount());
    dxf.addText(0.5, W - 1.2, 0.45, QStringLiteral("LABEL_NO_CUT"),
                QStringLiteral("SHEET %1 OF %2. CUT_OUTER and CUT_INNER only. Border and labels are not cut paths.")
                    .arg(sheet + 1)
                    .arg(sheets));
    return dxf.save(path, error);
}

bool writeFrameDxf(const QString& path, const TableModel& model, QString& error) {
    DxfWriter dxf;
    const TableSpec& s = model.spec();
    dxf.addPolyline({QPointF(0, 0), QPointF(s.length, 0), QPointF(s.length, s.width), QPointF(0, s.width)},
                    QStringLiteral("REF_NO_CUT"));
    for (const FrameRect& fr : model.frame()) {
        const QRectF& r = fr.rect;
        const QString layer = fr.role == 2 ? QStringLiteral("LABEL_NO_CUT") : QStringLiteral("REF_NO_CUT");
        dxf.addPolyline({r.topLeft(), r.topRight(), r.bottomRight(), r.bottomLeft()}, layer);
    }
    dxf.addText(0, s.width + 2.0, 0.55, QStringLiteral("LABEL_NO_CUT"),
                QStringLiteral("LEG PLAN ONLY - LEG %1 in  STRINGER %2 in  WALL %3 - INCHES. NOT A CUT FILE.")
                    .arg(QString::number(s.tubeSize, 'f', 3), QString::number(s.stringerSize, 'f', 3),
                         QString::number(s.tubeWall, 'f', 4)));
    dxf.addText(0, -2.2, 0.4, QStringLiteral("LABEL_NO_CUT"),
                (s.doubleStringers
                     ? QStringLiteral("Two long stringers for a shelf, each %1 in. Cross stringers %2 in. Stringer bottom %3 in off the floor.")
                           .arg(QString::number(model.railLength(), 'f', 3), QString::number(model.crossmemberLength(), 'f', 3),
                                QString::number(s.stringerHeight, 'f', 3))
                     : QStringLiteral("Center stringer is cut between the cross tubes. Cross stringers %1 in. Stringer bottom %2 in off the floor.")
                           .arg(QString::number(model.crossmemberLength(), 'f', 3), QString::number(s.stringerHeight, 'f', 3))));
    return dxf.save(path, error);
}
