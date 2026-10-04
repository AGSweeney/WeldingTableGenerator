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

#include "DxfAudit.h"

#include "AppVersion.h"
#include "PackageWriter.h"
#include "TableModel.h"

#include "drw_interface.h"
#include "libdxfrw.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

struct Feat {
    QString layer;
    bool circle = false;
    bool closed = false;
    bool bulge = false;
    double x = 0;
    double y = 0;
    double r = 0;
    std::vector<QPointF> pts;
};

class DxfReader : public DRW_Interface {
public:
    void addHeader(const DRW_Header* data) override {
        if (!data) {
            return;
        }
        const auto units = data->vars.find("$INSUNITS");
        if (units != data->vars.end() && units->second && units->second->type() == DRW_Variant::INTEGER) {
            m_units = units->second->content.i;
        }
        const auto version = data->vars.find("$ACADVER");
        if (version != data->vars.end() && version->second && version->second->type() == DRW_Variant::STRING &&
            version->second->content.s) {
            m_version = QString::fromStdString(*version->second->content.s);
        }
    }
    void addCircle(const DRW_Circle& data) override {
        Feat feat;
        feat.layer = QString::fromStdString(data.layer);
        feat.circle = true;
        feat.x = data.basePoint.x;
        feat.y = data.basePoint.y;
        feat.r = data.radious;
        m_feats.push_back(feat);
    }
    void addLWPolyline(const DRW_LWPolyline& data) override {
        Feat feat;
        feat.layer = QString::fromStdString(data.layer);
        feat.closed = (data.flags & 1) != 0;
        for (const std::shared_ptr<DRW_Vertex2D>& vertex : data.vertlist) {
            if (!vertex) {
                continue;
            }
            feat.pts.push_back(QPointF(vertex->x, vertex->y));
            if (std::abs(vertex->bulge) > 1e-6) {
                feat.bulge = true;
            }
        }
        m_feats.push_back(feat);
    }

    void writeHeader(DRW_Header&) override {}
    void writeBlocks() override {}
    void writeBlockRecords() override {}
    void writeEntities() override {}
    void writeLTypes() override {}
    void writeLayers() override {}
    void writeTextstyles() override {}
    void writeVports() override {}
    void writeDimstyles() override {}
    void writeObjects() override {}
    void writeAppId() override {}
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
    void addPolyline(const DRW_Polyline&) override {}
    void addPlotSettings(const DRW_PlotSettings*) override {}

    int m_units = -1;
    QString m_version;
    std::vector<Feat> m_feats;
};

bool segmentsCross(const QPointF& a, const QPointF& b, const QPointF& c, const QPointF& d) {
    const auto cross = [](const QPointF& u, const QPointF& v) { return u.x() * v.y() - u.y() * v.x(); };
    const QPointF r(b.x() - a.x(), b.y() - a.y());
    const QPointF s(d.x() - c.x(), d.y() - c.y());
    const double den = cross(r, s);
    if (std::abs(den) < 1e-12) {
        return false;
    }
    const QPointF ac(c.x() - a.x(), c.y() - a.y());
    const double t = cross(ac, s) / den;
    const double u = cross(ac, r) / den;
    constexpr double eps = 1e-7;
    return t > eps && t < 1.0 - eps && u > eps && u < 1.0 - eps;
}

bool selfIntersects(const std::vector<QPointF>& pts) {
    const int n = static_cast<int>(pts.size());
    if (n < 4) {
        return false;
    }
    for (int i = 0; i < n; ++i) {
        const QPointF a = pts[static_cast<size_t>(i)];
        const QPointF b = pts[static_cast<size_t>((i + 1) % n)];
        for (int j = i + 1; j < n; ++j) {
            const int gap = std::min(std::abs(j - i), n - std::abs(j - i));
            if (gap <= 1) {
                continue;
            }
            const QPointF c = pts[static_cast<size_t>(j)];
            const QPointF d = pts[static_cast<size_t>((j + 1) % n)];
            if (segmentsCross(a, b, c, d)) {
                return true;
            }
        }
    }
    return false;
}

QRectF boundsOf(const std::vector<QPointF>& pts) {
    if (pts.empty()) {
        return {};
    }
    double minX = pts.front().x();
    double maxX = minX;
    double minY = pts.front().y();
    double maxY = minY;
    for (const QPointF& p : pts) {
        minX = std::min(minX, p.x());
        maxX = std::max(maxX, p.x());
        minY = std::min(minY, p.y());
        maxY = std::max(maxY, p.y());
    }
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

bool readDxf(const QString& path, DxfReader& reader, QString& error) {
    const QByteArray encoded = QFile::encodeName(path);
    dxfRW file(encoded.constData());
    if (!file.read(&reader, false)) {
        error = QStringLiteral("Could not read %1").arg(path);
        return false;
    }
    if (reader.m_units != 1) {
        error = QStringLiteral("%1 units are %2, expected inches (1)").arg(path).arg(reader.m_units);
        return false;
    }
    if (!reader.m_version.contains(QStringLiteral("AC1018"))) {
        error = QStringLiteral("%1 version is %2, expected AC1018").arg(path, reader.m_version);
        return false;
    }
    return true;
}

QString auditDrawing(const QString& path, bool cutFile, bool nestFile, const TableSpec* sheet) {
    DxfReader reader;
    QString error;
    if (!readDxf(path, reader, error)) {
        return error;
    }
    int outers = 0;
    QRectF outerBox;
    for (const Feat& feat : reader.m_feats) {
        const bool cut = feat.layer == QLatin1String("CUT_OUTER") || feat.layer == QLatin1String("CUT_INNER");
        if (cutFile && feat.layer == QLatin1String("CUT_OUTER") && !feat.circle) {
            ++outers;
            if (!feat.closed || feat.pts.size() < 3) {
                return QStringLiteral("%1 outer contour is not a closed polyline").arg(path);
            }
            if (!feat.bulge && selfIntersects(feat.pts)) {
                return QStringLiteral("%1 outer contour crosses itself").arg(path);
            }
            outerBox = boundsOf(feat.pts);
        }
        if (cut && feat.circle && feat.r <= 1e-6) {
            return QStringLiteral("%1 has a zero-radius circle").arg(path);
        }
        if (cut && !feat.circle && feat.pts.size() >= 4 && feat.closed && !feat.bulge && selfIntersects(feat.pts)) {
            return QStringLiteral("%1 has a self-intersecting %2 contour").arg(path, feat.layer);
        }
    }
    if (cutFile && outers < 1) {
        return QStringLiteral("%1 has no closed CUT_OUTER contour").arg(path);
    }
    if (!cutFile) {
        for (const Feat& feat : reader.m_feats) {
            if (feat.layer == QLatin1String("CUT_OUTER") || feat.layer == QLatin1String("CUT_INNER")) {
                return QStringLiteral("%1 reference drawing contains a cut layer").arg(path);
            }
        }
    }
    if (nestFile && sheet) {
        bool border = false;
        for (const Feat& feat : reader.m_feats) {
            if (feat.layer != QLatin1String("REF_NO_CUT") || feat.circle || feat.pts.size() < 4) {
                continue;
            }
            const QRectF box = boundsOf(feat.pts);
            if (std::abs(box.width() - sheet->sheetLength) < 0.05 && std::abs(box.height() - sheet->sheetWidth) < 0.05) {
                border = true;
            }
        }
        if (!border) {
            return QStringLiteral("%1 nest border is not the %2 x %3 sheet")
                .arg(path)
                .arg(sheet->sheetLength)
                .arg(sheet->sheetWidth);
        }
        for (const Feat& feat : reader.m_feats) {
            const bool cut = feat.layer == QLatin1String("CUT_OUTER") || feat.layer == QLatin1String("CUT_INNER");
            if (!cut) {
                continue;
            }
            if (feat.circle) {
                if (feat.x - feat.r < -0.02 || feat.y - feat.r < -0.02 || feat.x + feat.r > sheet->sheetLength + 0.02 ||
                    feat.y + feat.r > sheet->sheetWidth + 0.02) {
                    return QStringLiteral("%1 nest circle leaves the sheet").arg(path);
                }
            } else {
                for (const QPointF& p : feat.pts) {
                    if (p.x() < -0.02 || p.y() < -0.02 || p.x() > sheet->sheetLength + 0.02 ||
                        p.y() > sheet->sheetWidth + 0.02) {
                        return QStringLiteral("%1 nest contour leaves the sheet").arg(path);
                    }
                }
            }
        }
    }
    if (cutFile && !nestFile && outerBox.isNull()) {
        return QStringLiteral("%1 has an empty outer contour").arg(path);
    }
    return {};
}

QString auditPackage(const TableModel& model, const QString& packageDir) {
    const QString manifestPath = QDir(packageDir).filePath(QStringLiteral("Package_Manifest.json"));
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        return QStringLiteral("Missing Package_Manifest.json in %1").arg(packageDir);
    }
    const QJsonObject manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
    if (manifest.value(QStringLiteral("application_version")).toString() != QLatin1String(kAppVersion)) {
        return QStringLiteral("Manifest version mismatch");
    }
    if (manifest.value(QStringLiteral("job_id")).toString().isEmpty()) {
        return QStringLiteral("Manifest has no job id");
    }
    if (manifest.value(QStringLiteral("revision")).toString() != model.spec().revision) {
        return QStringLiteral("Manifest revision mismatch");
    }
    if (manifest.value(QStringLiteral("settings_sha256")).toString() != packageSettingsHash(model.spec())) {
        return QStringLiteral("Manifest settings hash mismatch");
    }
    if (!QFileInfo(packageDir).fileName().contains(manifest.value(QStringLiteral("job_id")).toString())) {
        return QStringLiteral("Package folder does not contain the job id");
    }

    const QDir dxfDir(QDir(packageDir).filePath(QStringLiteral("DXF")));
    const QStringList names = dxfDir.entryList({QStringLiteral("*.dxf")}, QDir::Files);
    const TableSpec& spec = model.spec();
    if (spec.writeIndividuals) {
        for (const Part& part : model.parts()) {
            if (part.qty < 1) {
                continue;
            }
            const QString prefix = part.code + QLatin1Char('_');
            QString match;
            for (const QString& name : names) {
                if (name.startsWith(prefix)) {
                    match = name;
                    break;
                }
            }
            if (match.isEmpty()) {
                return QStringLiteral("Missing DXF for %1").arg(part.code);
            }
            if (!match.contains(QStringLiteral("QTY%1").arg(part.qty)) ||
                !match.contains(QStringLiteral("Rev%1").arg(packageRevisionToken(spec.revision)))) {
                return QStringLiteral("%1 filename is missing the revision or quantity").arg(match);
            }
            const QString problem = auditDrawing(dxfDir.filePath(match), true, false, nullptr);
            if (!problem.isEmpty()) {
                return problem;
            }
            DxfReader reader;
            QString error;
            if (!readDxf(dxfDir.filePath(match), reader, error)) {
                return error;
            }
            for (const Feat& feat : reader.m_feats) {
                if (feat.layer == QLatin1String("CUT_OUTER") && !feat.circle) {
                    const QRectF box = boundsOf(feat.pts);
                    if (std::abs(box.width() - part.bounds.width()) > 0.02 ||
                        std::abs(box.height() - part.bounds.height()) > 0.02) {
                        return QStringLiteral("%1 outer size %2 x %3 does not match the part %4 x %5")
                            .arg(match)
                            .arg(box.width())
                            .arg(box.height())
                            .arg(part.bounds.width())
                            .arg(part.bounds.height());
                    }
                }
            }
        }
    }
    if (spec.writeNest) {
        if (!model.nestOk()) {
            return QStringLiteral("Nest was requested but the package was still written");
        }
        int nests = 0;
        for (const QString& name : names) {
            if (!name.contains(QStringLiteral("Sheet_Nest"))) {
                continue;
            }
            ++nests;
            const QString problem = auditDrawing(dxfDir.filePath(name), true, true, &spec);
            if (!problem.isEmpty()) {
                return problem;
            }
        }
        if (nests != model.nestSheetCount()) {
            return QStringLiteral("Expected %1 nest sheets, found %2").arg(model.nestSheetCount()).arg(nests);
        }
    }
    if (spec.writeFrame && spec.frame) {
        bool frame = false;
        for (const QString& name : names) {
            if (!name.contains(QStringLiteral("REFERENCE_ONLY"))) {
                continue;
            }
            frame = true;
            const QString problem = auditDrawing(dxfDir.filePath(name), false, false, nullptr);
            if (!problem.isEmpty()) {
                return problem;
            }
        }
        if (!frame) {
            return QStringLiteral("Missing reference frame DXF");
        }
    }
    return {};
}

TableSpec fast(TableSpec spec) {
    spec.writePdf = false;
    return spec;
}

}  // namespace

int runGeometrySweep(QString& report) {
    QStringList lines;
    int failed = 0;
    auto note = [&](bool ok, const QString& message) {
        lines << (ok ? QStringLiteral("OK ") : QStringLiteral("FAIL ")) + message;
        if (!ok) {
            ++failed;
        }
    };

    QTemporaryDir root;
    if (!root.isValid()) {
        report = QStringLiteral("Could not create a temporary folder");
        return 1;
    }
    const QString sentinelPath = QDir(root.path()).filePath(QStringLiteral("DXF/KEEP_ME.dxf"));
    QDir().mkpath(QFileInfo(sentinelPath).absolutePath());
    QFile sentinel(sentinelPath);
    if (!sentinel.open(QIODevice::WriteOnly)) {
        report = QStringLiteral("Could not write the sentinel DXF");
        return 1;
    }
    sentinel.write("user file\n");
    sentinel.close();

    auto exported = [&](const QString& name, const TableSpec& spec, bool mustExport) {
        TableModel model;
        const bool built = model.rebuild(spec);
        const auto partials = [&] {
            return QDir(root.path()).entryList({QStringLiteral(".partial-*")}, QDir::Dirs | QDir::Hidden);
        };
        if (!model.errors().isEmpty() || !built) {
            note(!mustExport, name + QStringLiteral(" blocked: ") + model.errors().join(QStringLiteral("; ")));
            note(QFileInfo::exists(sentinelPath), name + QStringLiteral(" left the existing DXF in place"));
            note(partials().isEmpty(), name + QStringLiteral(" left no partial folder"));
            return;
        }
        const PackageResult result = writePackage(model, root.path());
        if (!result.ok) {
            note(!mustExport, name + QStringLiteral(" package blocked: ") + result.message);
            note(QFileInfo::exists(sentinelPath), name + QStringLiteral(" left the existing DXF in place"));
            note(partials().isEmpty(), name + QStringLiteral(" left no partial folder"));
            return;
        }
        note(mustExport, name + QStringLiteral(" wrote ") + result.packageDir);
        const QString problem = auditPackage(model, result.packageDir);
        note(problem.isEmpty(), problem.isEmpty() ? name + QStringLiteral(" DXF audit") : name + QStringLiteral(" ") + problem);
        note(QFileInfo::exists(sentinelPath), name + QStringLiteral(" did not delete KEEP_ME.dxf"));
        note(partials().isEmpty(), name + QStringLiteral(" left no partial folder"));
    };

    TableSpec rev = TableSpec::revA();
    rev.writePdf = true;
    exported(QStringLiteral("Rev A"), rev, true);

    TableSpec wide = fast(TableSpec::revA());
    wide.length = 72;
    wide.width = 36;
    exported(QStringLiteral("36 x 72"), wide, true);

    TableSpec large = fast(TableSpec::revA());
    large.length = 96;
    large.width = 48;
    exported(QStringLiteral("48 x 96"), large, true);

    TableSpec inches = fast(TableSpec::revA());
    inches.holeDiameterInch = true;
    inches.holeDiameterMm = 0.625 * 25.4;
    exported(QStringLiteral("5/8 in holes"), inches, true);

    TableSpec inset = fast(TableSpec::revA());
    inset.apronInset = 0;
    exported(QStringLiteral("flush apron"), inset, true);

    TableSpec plain = fast(TableSpec::revA());
    plain.lightening = false;
    exported(QStringLiteral("no lightening"), plain, true);

    TableSpec legsOff = fast(TableSpec::revA());
    legsOff.frame = false;
    legsOff.writeFrame = false;
    exported(QStringLiteral("no frame"), legsOff, true);

    TableSpec multi = fast(TableSpec::revA());
    multi.sheetWidth = 36;
    exported(QStringLiteral("36 in sheet"), multi, true);

    TableSpec individuals = fast(TableSpec::revA());
    individuals.sheetLength = 10;
    individuals.sheetWidth = 10;
    individuals.writeNest = false;
    exported(QStringLiteral("nest not requested"), individuals, true);

    TableSpec nestFail = fast(TableSpec::revA());
    nestFail.sheetLength = 10;
    nestFail.sheetWidth = 10;
    exported(QStringLiteral("nest does not fit"), nestFail, false);

    TableSpec overlap = fast(TableSpec::revA());
    overlap.holePitchX = 0.4;
    overlap.holePitchY = 0.4;
    exported(QStringLiteral("overlapping holes"), overlap, false);

    const int packages = QDir(root.path()).entryList({QStringLiteral("WT_*")}, QDir::Dirs).size();
    note(packages >= 8, QStringLiteral("unique package folders %1").arg(packages));

    report = lines.join(QStringLiteral("\n")) + QLatin1Char('\n');
    report += failed == 0 ? QStringLiteral("SWEEP OK\n") : QStringLiteral("SWEEP FAILED\n");
    return failed == 0 ? 0 : 1;
}
