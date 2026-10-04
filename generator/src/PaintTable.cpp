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

#include "PaintTable.h"

#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <vector>

constexpr double kNestBar = 8.0;
constexpr double kNestBand = 18.0;
constexpr double kNestHeader = 6.0;
constexpr double kNestGroupGap = 4.0;

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Pal {
    QColor bg;
    QColor plate;
    QColor edge;
    QColor hole;
    QColor holeEdge;
    QColor apron;
    QColor rib;
    QColor frame;
    QColor leg;
    QColor stringer;
    QColor text;
    QColor muted;
    QColor grid;
};

Pal palette(bool dark) {
    Pal p;
    if (dark) {
        // On-screen canvas, matching the ANest nest sheet.
        p.bg = QColor(0xe8, 0xea, 0xed);
        p.plate = QColor(0xf7, 0xf8, 0xf8);
        p.edge = QColor(0x2b, 0x2b, 0x2b);
        p.hole = QColor(0xe8, 0xea, 0xed);
        p.holeEdge = QColor(0x2a, 0x5a, 0x8a);
        p.apron = QColor(0xb7, 0xc4, 0xce);
        p.rib = QColor(0xc5, 0xd5, 0xc0);
        p.frame = QColor(0xd5, 0xdb, 0xe0);
        p.leg = QColor(0x3d, 0x6b, 0x3d);
        p.stringer = QColor(0x5c, 0x6e, 0x7a);
        p.text = QColor(0x2b, 0x2b, 0x2b);
        p.muted = QColor(0x5a, 0x5a, 0x5a);
        p.grid = QColor(0xc5, 0xc8, 0xcc);
    } else {
        p.bg = Qt::white;
        p.plate = QColor(237, 243, 245);
        p.edge = QColor(24, 50, 68);
        p.hole = Qt::white;
        p.holeEdge = QColor(24, 50, 68);
        p.apron = QColor(121, 159, 172);
        p.rib = QColor(184, 213, 217);
        p.frame = QColor(220, 233, 236);
        p.leg = QColor(22, 133, 141);
        p.stringer = QColor(70, 96, 112);
        p.text = QColor(24, 50, 68);
        p.muted = QColor(104, 121, 133);
        p.grid = QColor(213, 224, 229);
    }
    return p;
}

QPen cosmetic(const QColor& color, qreal width, Qt::PenStyle style = Qt::SolidLine) {
    QPen pen(color, width, style);
    pen.setCosmetic(true);
    return pen;
}

void drawLabel(QPainter& p, const QPointF& world, const QString& text, const QColor& color, int px, int align) {
    const QPointF s = p.transform().map(world);
    p.save();
    p.resetTransform();
    p.setPen(color);
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(px);
    p.setFont(f);
    QRectF r(s.x() - 220, s.y() - 18, 440, 36);
    p.drawText(r, align | Qt::TextDontClip, text);
    p.restore();
}

void drawAnchoredLabel(QPainter& p, const QPointF& world, const QString& text, const QColor& color, int px, int align) {
    const QPointF s = p.transform().map(world);
    p.save();
    p.resetTransform();
    p.setPen(color);
    QFont font(QStringLiteral("Segoe UI"));
    font.setPixelSize(px);
    p.setFont(font);
    const QFontMetrics metrics(font);
    const int width = metrics.horizontalAdvance(text) + 4;
    const int height = metrics.height();
    double x = s.x();
    double y = s.y() - height * 0.5;
    if (align & Qt::AlignHCenter) {
        x -= width * 0.5;
    } else if (align & Qt::AlignRight) {
        x -= width;
    }
    if (align & Qt::AlignBottom) {
        y = s.y() - height;
    } else if (align & Qt::AlignTop) {
        y = s.y();
    }
    const int horizontal = align & Qt::AlignHorizontal_Mask;
    p.drawText(QRectF(x, y, width, height), Qt::AlignVCenter | horizontal, text);
    p.restore();
}

QString nestPieceLabel(const TubePiece& piece) {
    QString name = piece.name;
    if (name.startsWith(QLatin1String("Cross"))) {
        name = QStringLiteral("Cross");
    } else if (name.startsWith(QLatin1String("Center"))) {
        name = QStringLiteral("Center");
    } else if (name.startsWith(QLatin1String("Long"))) {
        name = QStringLiteral("Long");
    }
    return QStringLiteral("%1  %2").arg(name, QString::number(piece.length, 'f', 2));
}

void drawNestPieceLabels(QPainter& painter, const TubeStick& stick, double barBottom, const QColor& color) {
    QFont font(QStringLiteral("Segoe UI"));
    font.setPixelSize(10);
    const QFontMetrics metrics(font);
    const int line = metrics.height() + 2;
    const QTransform saved = painter.transform();
    struct Lab {
        QString text;
        double left = 0;
        double width = 0;
        int row = 0;
    };
    std::vector<Lab> labs;
    double x = 0;
    for (const TubePiece& piece : stick.pieces) {
        Lab lab;
        lab.text = nestPieceLabel(piece);
        lab.width = metrics.horizontalAdvance(lab.text);
        const double center = saved.map(QPointF(x + piece.length * 0.5, barBottom)).x();
        lab.left = center - lab.width * 0.5;
        labs.push_back(lab);
        x += piece.length + kTubeKerf;
    }
    std::vector<double> rowEnds;
    for (Lab& lab : labs) {
        int row = 0;
        while (row < static_cast<int>(rowEnds.size()) && lab.left < rowEnds[static_cast<size_t>(row)] + 8.0) {
            ++row;
        }
        if (row == static_cast<int>(rowEnds.size())) {
            rowEnds.push_back(lab.left + lab.width);
        } else {
            rowEnds[static_cast<size_t>(row)] = lab.left + lab.width;
        }
        lab.row = row;
    }
    const double top = saved.map(QPointF(0, barBottom)).y() + 2.0;
    painter.save();
    painter.resetTransform();
    painter.setFont(font);
    painter.setPen(color);
    for (const Lab& lab : labs) {
        painter.drawText(QRectF(lab.left, top + lab.row * line, lab.width + 2, metrics.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, lab.text);
    }
    painter.restore();
}

void drawDeviceText(QPainter& p, const QRectF& rect, const QString& text, const QColor& color, int px, int align) {
    p.save();
    p.resetTransform();
    p.setPen(color);
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(px);
    p.setFont(f);
    p.drawText(rect, align | Qt::TextWordWrap, text);
    p.restore();
}

std::vector<QPointF> capsulePoly(const CapsuleFeat& c) {
    std::vector<QPointF> pts;
    const double r = c.height * 0.5;
    if (c.length <= c.height + 1e-9) {
        for (int i = 0; i < 28; ++i) {
            const double a = 2 * kPi * i / 28.0;
            pts.emplace_back(c.x + r * std::cos(a), c.y + r * std::sin(a));
        }
        return pts;
    }
    const double a0 = c.x - c.length * 0.5 + r;
    const double b0 = c.x + c.length * 0.5 - r;
    for (int i = 0; i <= 14; ++i) {
        const double ang = -kPi * 0.5 + kPi * i / 14.0;
        pts.emplace_back(b0 + r * std::cos(ang), c.y + r * std::sin(ang));
    }
    for (int i = 0; i <= 14; ++i) {
        const double ang = kPi * 0.5 + kPi * i / 14.0;
        pts.emplace_back(a0 + r * std::cos(ang), c.y + r * std::sin(ang));
    }
    return pts;
}

void paintCutouts(QPainter& p, const Part& part, const Pal& pal) {
    p.setBrush(pal.hole);
    p.setPen(cosmetic(pal.holeEdge, 1.1));
    for (const CircleFeat& h : part.holes) {
        p.drawEllipse(QPointF(h.x, h.y), h.r, h.r);
    }
    for (const SlotFeat& s : part.slotCuts) {
        p.drawRect(QRectF(QPointF(s.x0, s.y0), QPointF(s.x1, s.y1)));
    }
    for (const CapsuleFeat& c : part.caps) {
        const auto poly = capsulePoly(c);
        QPolygonF q;
        for (const QPointF& pt : poly) {
            q << pt;
        }
        p.drawPolygon(q);
    }
}

void paintOutline(QPainter& p, const Part& part, const QColor& fill, const Pal& pal) {
    QPolygonF q;
    for (const QPointF& pt : part.outer) {
        q << pt;
    }
    p.setBrush(fill);
    p.setPen(cosmetic(pal.edge, 1.35));
    p.drawPolygon(q);
    paintCutouts(p, part, pal);
}

struct Vec3 {
    double x = 0;
    double y = 0;
    double z = 0;
};

Vec3 vsub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
double vdot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 vcross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double vlen(Vec3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
Vec3 vnorm(Vec3 a) {
    const double n = vlen(a);
    if (n < 1e-12) {
        return {0, 0, 1};
    }
    return {a.x / n, a.y / n, a.z / n};
}

struct Cam {
    Vec3 center;
    Vec3 eye;
    Vec3 right;
    Vec3 up;
    QPointF project(Vec3 p) const {
        const Vec3 d = vsub(p, center);
        return QPointF(vdot(d, right), vdot(d, up));
    }
    double depth(Vec3 p) const { return vdot(vsub(p, center), eye); }
};

Cam makeCam(double yaw, double pitch, Vec3 center) {
    const double cp = std::cos(pitch);
    const double sp = std::sin(pitch);
    Cam cam;
    cam.center = center;
    cam.eye = {cp * std::cos(yaw), cp * std::sin(yaw), sp};
    double rx = -cam.eye.y;
    double ry = cam.eye.x;
    double rl = std::hypot(rx, ry);
    if (rl < 1e-6) {
        rx = -std::sin(yaw);
        ry = std::cos(yaw);
        if (sp < 0) {
            rx = -rx;
            ry = -ry;
        }
        rl = 1;
    }
    cam.right = {rx / rl, ry / rl, 0};
    const Vec3 forward{-cam.eye.x, -cam.eye.y, -cam.eye.z};
    cam.up = vnorm(vcross(cam.right, forward));
    return cam;
}

struct SceneZ {
    double foot = 0;
    double zWeb0 = 0;
    double zWeb1 = 0;
    double zTop1 = 0;
    bool framed = false;
    Vec3 center;
};

SceneZ sceneZ(const TableModel& model) {
    const TableSpec& s = model.spec();
    SceneZ z;
    z.framed = s.frame && !model.frame().empty();
    if (z.framed) {
        z.foot = std::max(0.02, s.topThickness);
        z.zTop1 = s.finishedHeight;
        z.zWeb1 = z.zTop1 - std::max(0.0, s.topThickness);
        z.zWeb0 = z.zWeb1 - std::max(0.0, s.webDepth);
    } else {
        z.zWeb0 = 0;
        z.zWeb1 = std::max(0.0, s.webDepth);
        z.zTop1 = z.zWeb1 + std::max(0.0, s.topThickness);
    }
    z.center = {s.length * 0.5, s.width * 0.5, z.zTop1 * 0.5};
    return z;
}

QRectF orbitFrame(const TableModel& model) {
    const SceneZ z = sceneZ(model);
    const TableSpec& s = model.spec();
    const double reach = 0.5 * std::sqrt(s.length * s.length + s.width * s.width + z.zTop1 * z.zTop1);
    const double half = reach * 1.08 + 2.0;
    return QRectF(-half, -half, half * 2, half * 2);
}

struct Face3 {
    QPolygonF poly;
    std::vector<double> vz;
    QColor fill;
};

QColor shade(const QColor& c, double lit) {
    const double k = std::clamp(0.38 + 0.72 * lit, 0.0, 1.15);
    auto ch = [&](int v) { return std::clamp(int(std::lround(v * k)), 0, 255); };
    return QColor(ch(c.red()), ch(c.green()), ch(c.blue()));
}

void fillTri(QImage& img, float* zbuf, float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2,
             QRgb color) {
    if (y0 > y1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
        std::swap(z0, z1);
    }
    if (y1 > y2) {
        std::swap(x1, x2);
        std::swap(y1, y2);
        std::swap(z1, z2);
    }
    if (y0 > y1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
        std::swap(z0, z1);
    }
    if (y2 - y0 < 1e-4f) {
        return;
    }
    const int W = img.width();
    const int H = img.height();
    const int yStart = std::max(0, static_cast<int>(std::ceil(y0 - 0.5f)));
    const int yEnd = std::min(H - 1, static_cast<int>(std::floor(y2 - 0.5f)));
    auto at = [](float y, float yA, float vA, float yB, float vB) {
        const float den = yB - yA;
        const float t = std::abs(den) < 1e-6f ? 0.f : (y - yA) / den;
        return vA + t * (vB - vA);
    };
    for (int y = yStart; y <= yEnd; ++y) {
        const float py = static_cast<float>(y) + 0.5f;
        const float xa = at(py, y0, x0, y2, x2);
        const float za = at(py, y0, z0, y2, z2);
        float xb;
        float zb;
        if (py < y1 || std::abs(y2 - y1) < 1e-6f) {
            xb = at(py, y0, x0, y1, x1);
            zb = at(py, y0, z0, y1, z1);
        } else {
            xb = at(py, y1, x1, y2, x2);
            zb = at(py, y1, z1, y2, z2);
        }
        float left = xa;
        float right = xb;
        float zl = za;
        float zr = zb;
        if (left > right) {
            std::swap(left, right);
            std::swap(zl, zr);
        }
        const int xStart = std::max(0, static_cast<int>(std::ceil(left - 0.5f)));
        const int xEnd = std::min(W - 1, static_cast<int>(std::floor(right - 0.5f)));
        if (xStart > xEnd) {
            continue;
        }
        QRgb* row = reinterpret_cast<QRgb*>(img.scanLine(y));
        float* zrow = zbuf + static_cast<size_t>(y) * static_cast<size_t>(W);
        const float span = right - left;
        for (int x = xStart; x <= xEnd; ++x) {
            const float t = span > 1e-5f ? (static_cast<float>(x) + 0.5f - left) / span : 0.f;
            const float z = zl + t * (zr - zl);
            if (z > zrow[x]) {
                zrow[x] = z;
                row[x] = color;
            }
        }
    }
}

void pushFace(std::vector<Face3>& faces, const Cam& cam, const Vec3* pts, int n, Vec3 outward, const QColor& base) {
    if (n < 3 || vdot(outward, cam.eye) <= 0.02) {
        return;
    }
    const Vec3 light = vnorm({-0.35, -0.45, 0.82});
    const double lit = std::clamp(vdot(outward, light), 0.0, 1.0);
    Face3 face;
    for (int i = 0; i < n; ++i) {
        face.poly << cam.project(pts[i]);
        face.vz.push_back(cam.depth(pts[i]));
    }
    face.fill = shade(base, lit);
    faces.push_back(std::move(face));
}

void addQuad(std::vector<Face3>& faces, const Cam& cam, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 outward, const QColor& base) {
    const Vec3 pts[4] = {a, b, c, d};
    pushFace(faces, cam, pts, 4, vnorm(outward), base);
}

void addBox(std::vector<Face3>& faces, const Cam& cam, double x0, double y0, double z0, double x1, double y1, double z1,
            const QColor& base) {
    if (x1 - x0 < 1e-4 || y1 - y0 < 1e-4 || z1 - z0 < 1e-4) {
        return;
    }
    addQuad(faces, cam, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}, {0, 0, 1}, base);
    addQuad(faces, cam, {x0, y1, z0}, {x1, y1, z0}, {x1, y0, z0}, {x0, y0, z0}, {0, 0, -1}, base);
    addQuad(faces, cam, {x1, y0, z0}, {x0, y0, z0}, {x0, y0, z1}, {x1, y0, z1}, {0, -1, 0}, base);
    addQuad(faces, cam, {x0, y1, z0}, {x1, y1, z0}, {x1, y1, z1}, {x0, y1, z1}, {0, 1, 0}, base);
    addQuad(faces, cam, {x0, y1, z0}, {x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {-1, 0, 0}, base);
    addQuad(faces, cam, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, {x1, y0, z1}, {1, 0, 0}, base);
}

void addTop(std::vector<Face3>& faces, const Cam& cam, const std::vector<QPointF>& outer, double z0, double z1,
            const QColor& base) {
    if (outer.size() < 3 || z1 - z0 < 1e-4) {
        return;
    }
    std::vector<Vec3> top;
    std::vector<Vec3> bot;
    top.reserve(outer.size());
    bot.reserve(outer.size());
    for (const QPointF& p : outer) {
        if (!top.empty() && std::hypot(p.x() - top.back().x, p.y() - top.back().y) < 1e-6) {
            continue;
        }
        top.push_back({p.x(), p.y(), z1});
        bot.push_back({p.x(), p.y(), z0});
    }
    if (top.size() >= 2 && std::hypot(top.front().x - top.back().x, top.front().y - top.back().y) < 1e-6) {
        top.pop_back();
        bot.pop_back();
    }
    if (top.size() < 3) {
        return;
    }
    pushFace(faces, cam, top.data(), static_cast<int>(top.size()), {0, 0, 1}, base);
    pushFace(faces, cam, bot.data(), static_cast<int>(bot.size()), {0, 0, -1}, base);
    for (size_t i = 0; i < top.size(); ++i) {
        const Vec3& a = bot[i];
        const Vec3& b = bot[(i + 1) % top.size()];
        const Vec3& c = top[(i + 1) % top.size()];
        const Vec3& d = top[i];
        const double dx = b.x - a.x;
        const double dy = b.y - a.y;
        addQuad(faces, cam, a, b, c, d, {dy, -dx, 0}, base);
    }
}

void addDisc(std::vector<Face3>& faces, const Cam& cam, double x, double y, double z, double r, Vec3 outward,
             const QColor& base, double bias) {
    if (r < 1e-4) {
        return;
    }
    Vec3 pts[16];
    for (int i = 0; i < 16; ++i) {
        const double a = 2 * kPi * i / 16.0;
        pts[i] = {x + r * std::cos(a), y + r * std::sin(a), z};
    }
    const size_t before = faces.size();
    pushFace(faces, cam, pts, 16, outward, base);
    if (faces.size() > before) {
        for (double& d : faces.back().vz) {
            d += bias;
        }
    }
}

void addDiscAxes(std::vector<Face3>& faces, const Cam& cam, Vec3 c, Vec3 u, Vec3 v, double r, Vec3 outward,
                 const QColor& base, double bias) {
    if (r < 1e-4) {
        return;
    }
    Vec3 pts[16];
    for (int i = 0; i < 16; ++i) {
        const double a = 2 * kPi * i / 16.0;
        const double ca = r * std::cos(a);
        const double sa = r * std::sin(a);
        pts[i] = {c.x + u.x * ca + v.x * sa, c.y + u.y * ca + v.y * sa, c.z + u.z * ca + v.z * sa};
    }
    const size_t before = faces.size();
    pushFace(faces, cam, pts, 16, outward, base);
    if (faces.size() > before) {
        for (double& d : faces.back().vz) {
            d += bias;
        }
    }
}

void paintSolid(QPainter& painter, const TableModel& model, const Pal& pal, double yaw, double pitch) {
    const TableSpec& s = model.spec();
    const SceneZ z = sceneZ(model);
    const Cam cam = makeCam(yaw, pitch, z.center);
    std::vector<Face3> faces;
    faces.reserve(800);

    const double t = std::max(0.05, s.webThickness);
    const double yFront = s.apronInset;
    const double yBack = s.width - s.apronInset - t;
    const double xLeft = s.apronInset;
    const double xRight = s.length - s.apronInset - t;
    addBox(faces, cam, s.apronInset, yFront, z.zWeb0, s.length - s.apronInset, yFront + t, z.zWeb1, pal.apron);
    addBox(faces, cam, s.apronInset, yBack, z.zWeb0, s.length - s.apronInset, yBack + t, z.zWeb1, pal.apron);
    const double yIn0 = s.apronInset + t;
    const double yIn1 = s.width - s.apronInset - t;
    if (yIn1 > yIn0) {
        addBox(faces, cam, xLeft, yIn0, z.zWeb0, xLeft + t, yIn1, z.zWeb1, pal.apron);
        addBox(faces, cam, xRight, yIn0, z.zWeb0, xRight + t, yIn1, z.zWeb1, pal.apron);
    }
    auto stampHole = [&](Vec3 c, Vec3 u, Vec3 v, Vec3 n, double r) {
        addDiscAxes(faces, cam, c, u, v, r, n, pal.holeEdge, 0.05);
        addDiscAxes(faces, cam, c, u, v, r * 0.62, n, pal.hole, 0.09);
    };
    if (const Part* longApron = model.find(QStringLiteral("P02"))) {
        const Vec3 u{1, 0, 0};
        const Vec3 v{0, 0, 1};
        for (const CircleFeat& h : longApron->holes) {
            const double x = s.apronInset + h.x;
            const double zz = z.zWeb1 - h.y;
            stampHole({x, yFront, zz}, u, v, {0, -1, 0}, h.r);
            stampHole({x, yFront + t, zz}, u, v, {0, 1, 0}, h.r);
            stampHole({x, yBack + t, zz}, u, v, {0, 1, 0}, h.r);
            stampHole({x, yBack, zz}, u, v, {0, -1, 0}, h.r);
        }
    }
    if (const Part* endApron = model.find(QStringLiteral("P03"))) {
        const Vec3 u{0, 1, 0};
        const Vec3 v{0, 0, 1};
        for (const CircleFeat& h : endApron->holes) {
            const double y = s.apronInset + t + h.x;
            const double zz = z.zWeb1 - h.y;
            stampHole({xLeft, y, zz}, u, v, {-1, 0, 0}, h.r);
            stampHole({xLeft + t, y, zz}, u, v, {1, 0, 0}, h.r);
            stampHole({xRight + t, y, zz}, u, v, {1, 0, 0}, h.r);
            stampHole({xRight, y, zz}, u, v, {-1, 0, 0}, h.r);
        }
    }
    for (double y : model.longY()) {
        addBox(faces, cam, model.ribOriginX(), y - t * 0.5, z.zWeb0, model.ribOriginX() + model.longRibLength(), y + t * 0.5,
               z.zWeb1, pal.rib);
    }
    for (double x : model.crossX()) {
        addBox(faces, cam, x - t * 0.5, model.ribOriginY(), z.zWeb0, x + t * 0.5, model.ribOriginY() + model.crossRibLength(),
               z.zWeb1, pal.rib);
    }

    if (z.framed) {
        const double zStr0 = std::max(0.0, s.stringerHeight);
        const double zStr1 = zStr0 + std::max(0.05, s.stringerSize);
        for (const FrameRect& fr : model.frame()) {
            if (fr.role != 3 && fr.role != 4) {
                continue;
            }
            const QRectF r = fr.rect.normalized();
            addBox(faces, cam, r.left(), r.top(), zStr0, r.right(), r.bottom(), zStr1, pal.stringer);
        }
        const QColor foot = pal.leg.darker(125);
        for (const FrameRect& fr : model.frame()) {
            if (fr.role != 2) {
                continue;
            }
            const QRectF r = fr.rect.normalized();
            addBox(faces, cam, r.left(), r.top(), z.foot, r.right(), r.bottom(), z.zWeb1, pal.leg);
            addBox(faces, cam, r.left() - 0.5, r.top() - 0.5, 0, r.right() + 0.5, r.bottom() + 0.5, z.foot, foot);
        }
    }

    const Part* top = model.find(QStringLiteral("P01"));
    const double zTop0 = z.zWeb1;
    if (top && !top->outer.empty()) {
        addTop(faces, cam, top->outer, zTop0, z.zTop1, pal.plate);
        const double hz = cam.eye.z >= 0 ? z.zTop1 : zTop0;
        const Vec3 hn = cam.eye.z >= 0 ? Vec3{0, 0, 1} : Vec3{0, 0, -1};
        for (const CircleFeat& h : top->holes) {
            addDisc(faces, cam, h.x, h.y, hz, h.r, hn, pal.holeEdge, 0.04);
            addDisc(faces, cam, h.x, h.y, hz, h.r * 0.62, hn, pal.hole, 0.08);
        }
        for (const SlotFeat& slot : top->slotCuts) {
            const double sz = hz;
            const size_t before = faces.size();
            addQuad(faces, cam, {slot.x0, slot.y0, sz}, {slot.x1, slot.y0, sz}, {slot.x1, slot.y1, sz},
                    {slot.x0, slot.y1, sz}, hn, pal.holeEdge);
            if (faces.size() > before) {
                for (double& d : faces.back().vz) {
                    d += 0.04;
                }
            }
            const double m = 0.03;
            const size_t inner = faces.size();
            addQuad(faces, cam, {slot.x0 + m, slot.y0 + m, sz}, {slot.x1 - m, slot.y0 + m, sz},
                    {slot.x1 - m, slot.y1 - m, sz}, {slot.x0 + m, slot.y1 - m, sz}, hn, pal.hole);
            if (faces.size() > inner) {
                for (double& d : faces.back().vz) {
                    d += 0.08;
                }
            }
        }
    } else {
        addBox(faces, cam, 0, 0, zTop0, s.length, s.width, z.zTop1, pal.plate);
    }

    QPaintDevice* device = painter.device();
    if (!device || faces.empty()) {
        return;
    }
    const int ss = 2;
    const int dw = device->width();
    const int dh = device->height();
    if (dw < 2 || dh < 2) {
        return;
    }
    QImage img(dw * ss, dh * ss, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    std::vector<float> zbuf(static_cast<size_t>(img.width()) * static_cast<size_t>(img.height()), -1.0e30f);
    const QTransform tr = painter.transform();

    for (const Face3& face : faces) {
        const int n = face.poly.size();
        if (n < 3 || static_cast<int>(face.vz.size()) != n) {
            continue;
        }
        std::vector<QPointF> screen(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            const QPointF spt = tr.map(face.poly[i]);
            screen[static_cast<size_t>(i)] = QPointF(spt.x() * ss, spt.y() * ss);
        }
        QPointF c(0, 0);
        double zc = 0;
        for (int i = 0; i < n; ++i) {
            c += screen[static_cast<size_t>(i)];
            zc += face.vz[static_cast<size_t>(i)];
        }
        c /= n;
        zc /= n;
        const QRgb fill = face.fill.rgb();
        for (int i = 0; i < n; ++i) {
            const int j = (i + 1) % n;
            const QPointF& a = screen[static_cast<size_t>(i)];
            const QPointF& b = screen[static_cast<size_t>(j)];
            fillTri(img, zbuf.data(), static_cast<float>(a.x()), static_cast<float>(a.y()),
                    static_cast<float>(face.vz[static_cast<size_t>(i)]), static_cast<float>(b.x()), static_cast<float>(b.y()),
                    static_cast<float>(face.vz[static_cast<size_t>(j)]), static_cast<float>(c.x()), static_cast<float>(c.y()),
                    static_cast<float>(zc), fill);
        }
    }

    painter.save();
    painter.resetTransform();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(QRect(0, 0, dw, dh), img);
    painter.restore();
}

}  // namespace

QTransform viewTransform(const QRectF& port, const QRectF& world, double zoom, const QPointF& pan, bool yUp) {
    QRectF w = world;
    if (w.width() < 1e-6) {
        w.setWidth(1);
    }
    if (w.height() < 1e-6) {
        w.setHeight(1);
    }
    const double s = std::min(port.width() / w.width(), port.height() / w.height()) * std::max(0.05, zoom);
    const QPointF c = w.center();
    QTransform tr;
    tr.translate(port.center().x() + pan.x(), port.center().y() + pan.y());
    tr.scale(s, yUp ? -s : s);
    tr.translate(-c.x(), -c.y());
    return tr;
}

QRectF viewWorldBounds(const TableModel& model, TableView view, int nestSheet) {
    const TableSpec& s = model.spec();
    switch (view) {
    case TableView::Plan:
        return QRectF(QPointF(-8, -8), QPointF(s.length + 8, s.width + 8));
    case TableView::Nest: {
        const int sheets = nestSheet >= 0 ? 1 : std::max(1, model.nestSheetCount());
        const double span = sheets * s.sheetWidth + (sheets > 1 ? (sheets - 1) * 10.0 : 0.0);
        return QRectF(QPointF(-4, -4), QPointF(s.sheetLength + 4, span + 6));
    }
    case TableView::Frame: {
        double bottom = -kNestHeader;
        double nestLength = s.length;
        for (const TubeNest& nest : model.tubeNests()) {
            nestLength = std::max(nestLength, nest.stockLength);
            bottom -= kNestHeader;
            bottom -= static_cast<double>(nest.sticks.size()) * (kNestBar + kNestBand);
            bottom -= kNestGroupGap;
        }
        return QRectF(QPointF(-6, bottom), QPointF(nestLength + 36, s.width + 10));
    }
    case TableView::Iso:
        return orbitFrame(model);
    case TableView::Profiles:
        if (const Part* p = model.find(QStringLiteral("P02"))) {
            return p->bounds.adjusted(-2, -2, 2, 2);
        }
        return QRectF(0, 0, 10, 10);
    }
    return QRectF(0, 0, 10, 10);
}

QPointF screenToWorld(const QRectF& port, const QRectF& world, double zoom, const QPointF& pan, const QPointF& screen) {
    return viewTransform(port, world, zoom, pan, true).inverted().map(screen);
}

void paintTable(QPainter& painter, const QRectF& port, const TableModel& model, TableView view, bool dark, double zoom,
                const QPointF& pan, double yaw, double pitch, int nestSheet) {
    const Pal pal = palette(dark);
    painter.save();
    painter.setClipRect(port);
    painter.fillRect(port, pal.bg);
    const TableSpec& spec = model.spec();

    if (view == TableView::Profiles) {
        const struct {
            const char* code;
        } rows[] = {{"P02"}, {"P04"}, {"P03"}, {"P05"}};
        const double bandH = port.height() / 4.0;
        for (int i = 0; i < 4; ++i) {
            const QRectF band(port.left() + 8, port.top() + i * bandH + 4, port.width() - 16, bandH - 8);
            const Part* part = model.find(QString::fromLatin1(rows[i].code));
            drawDeviceText(painter, QRectF(band.left(), band.top(), band.width(), 22),
                           part ? QStringLiteral("%1   %2    Qty %3    %4 in thick")
                                       .arg(part->code, part->name)
                                       .arg(part->qty)
                                       .arg(QString::number(part->thickness, 'f', 3))
                                 : QStringLiteral("Missing part"),
                           pal.text, 13, Qt::AlignLeft | Qt::AlignVCenter);
            if (!part || part->outer.empty()) {
                continue;
            }
            const QRectF canvas(band.left(), band.top() + 24, band.width(), band.height() - 24);
            const QRectF world = part->bounds.adjusted(-1.5, -0.8, 1.5, 0.8);
            painter.save();
            painter.setTransform(viewTransform(canvas, world, zoom, pan, false), true);
            paintOutline(painter, *part, i % 2 ? pal.rib : pal.apron, pal);
            painter.restore();
        }
        painter.restore();
        return;
    }

    const bool yUp = view != TableView::Profiles;
    const QRectF world = viewWorldBounds(model, view, nestSheet);
    painter.save();
    painter.setTransform(viewTransform(port, world, zoom, pan, yUp), true);

    if (view == TableView::Plan) {
        const Part* top = model.find(QStringLiteral("P01"));
        if (top) {
            paintOutline(painter, *top, pal.plate, pal);
        }
        painter.setPen(cosmetic(pal.muted, 1.0, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        const double ay0 = spec.apronInset;
        const double ay1 = spec.width - spec.apronInset;
        const double ax0 = spec.apronInset;
        const double ax1 = spec.length - spec.apronInset;
        painter.drawLine(QPointF(0, ay0), QPointF(spec.length, ay0));
        painter.drawLine(QPointF(0, ay1), QPointF(spec.length, ay1));
        painter.drawLine(QPointF(ax0, 0), QPointF(ax0, spec.width));
        painter.drawLine(QPointF(ax1, 0), QPointF(ax1, spec.width));
        painter.setPen(cosmetic(pal.leg, 1.15, Qt::DashLine));
        for (double y : model.longY()) {
            painter.drawLine(QPointF(0, y), QPointF(spec.length, y));
        }
        for (double x : model.crossX()) {
            painter.drawLine(QPointF(x, 0), QPointF(x, spec.width));
        }
        drawLabel(painter, QPointF(spec.length * 0.5, -3.2),
                  QStringLiteral("%1 in").arg(QString::number(spec.length, 'f', 3)), pal.text, 14, Qt::AlignHCenter);
        drawLabel(painter, QPointF(-3.4, spec.width * 0.5),
                  QStringLiteral("%1 in").arg(QString::number(spec.width, 'f', 3)), pal.text, 14, Qt::AlignHCenter);
        drawLabel(painter, QPointF(spec.length * 0.5, spec.width + 3.2),
                  QStringLiteral("%1 holes   %2 slots   %3 dog holes")
                      .arg(model.topHoleCount())
                      .arg(model.topSlotCount())
                      .arg(model.dogHoleLabel()),
                  pal.muted, 13, Qt::AlignHCenter);
        if (!model.tabHoleHits().empty()) {
            const QColor warn(176, 32, 32);
            painter.setPen(cosmetic(warn, 2.4));
            painter.setBrush(QColor(176, 32, 32, 80));
            for (const SlotFeat& hit : model.tabHoleHits()) {
                painter.drawRect(QRectF(QPointF(hit.x0, hit.y0), QPointF(hit.x1, hit.y1)));
            }
            drawLabel(painter, QPointF(spec.length * 0.5, spec.width + 6.2),
                      QStringLiteral("Apron tab cuts a dog hole (%1)").arg(model.tabHoleHits().size()), warn, 14,
                      Qt::AlignHCenter);
        }
    } else if (view == TableView::Nest) {
        painter.setBrush(dark ? QColor(28, 34, 44) : QColor(250, 250, 250));
        painter.setPen(cosmetic(pal.edge, 1.4));
        if (!model.nestOk()) {
            painter.drawRect(QRectF(0, 0, spec.sheetLength, spec.sheetWidth));
            painter.restore();
            drawDeviceText(painter, port.adjusted(24, 24, -24, -24), model.nestError(), pal.holeEdge, 16,
                           Qt::AlignCenter);
            painter.restore();
            return;
        }
        const int sheetCount = std::max(1, model.nestSheetCount());
        const bool stack = nestSheet < 0 && sheetCount > 1;
        const double pitch = spec.sheetWidth + 10.0;
        const int first = nestSheet < 0 ? 0 : std::clamp(nestSheet, 0, sheetCount - 1);
        const int last = nestSheet < 0 ? sheetCount - 1 : first;
        for (int sheet = first; sheet <= last; ++sheet) {
            const double y0 = stack ? (sheetCount - 1 - sheet) * pitch : 0.0;
            painter.drawRect(QRectF(0, y0, spec.sheetLength, spec.sheetWidth));
            int onSheet = 0;
            for (const Placement& pl : model.nest()) {
                if (pl.sheet == sheet) {
                    ++onSheet;
                }
            }
            const QString caption = sheetCount == 1
                                        ? QStringLiteral("%1 x %2 in sheet    gap %3    %4 parts")
                                              .arg(QString::number(spec.sheetLength, 'f', 1),
                                                   QString::number(spec.sheetWidth, 'f', 1),
                                                   QString::number(spec.nestGap, 'f', 3))
                                              .arg(onSheet)
                                        : QStringLiteral("Sheet %1 of %2    %3 x %4 in    %5 parts")
                                              .arg(sheet + 1)
                                              .arg(sheetCount)
                                              .arg(QString::number(spec.sheetLength, 'f', 1),
                                                   QString::number(spec.sheetWidth, 'f', 1))
                                              .arg(onSheet);
            drawLabel(painter, QPointF(spec.sheetLength * 0.5, y0 + spec.sheetWidth + 2.4), caption, pal.muted, 13,
                      Qt::AlignHCenter);
        }
        for (const Placement& pl : model.nest()) {
            if (pl.sheet < first || pl.sheet > last) {
                continue;
            }
            const Part& part = model.parts().at(static_cast<size_t>(pl.partIndex));
            const double yOff = stack ? (sheetCount - 1 - pl.sheet) * pitch : 0.0;
            painter.save();
            painter.translate(pl.dx, pl.dy + yOff);
            const QColor fill = part.code.startsWith(QLatin1String("Q")) ? pal.frame
                                : (part.code == QLatin1String("P04") || part.code == QLatin1String("P05")) ? pal.rib
                                                                                                          : pal.apron;
            paintOutline(painter, part, fill, pal);
            painter.restore();
            const QPointF at = part.bounds.translated(pl.dx, pl.dy + yOff).center();
            drawLabel(painter, at, pl.label, pal.text, 11, Qt::AlignCenter);
        }
    } else if (view == TableView::Frame) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(cosmetic(pal.edge, 1.3));
        painter.drawRect(QRectF(0, 0, spec.length, spec.width));
        const double web = spec.webThickness;
        painter.setPen(cosmetic(pal.muted, 1.0, Qt::DashLine));
        painter.drawRect(QRectF(spec.apronInset + web, spec.apronInset + web, spec.length - 2.0 * (spec.apronInset + web),
                                spec.width - 2.0 * (spec.apronInset + web)));
        for (const FrameRect& fr : model.frame()) {
            if (fr.role != 2) {
                continue;
            }
            painter.setBrush(pal.leg);
            painter.setPen(cosmetic(pal.edge, 1.2));
            painter.drawRect(fr.rect);
        }
        for (const FrameRect& fr : model.frame()) {
            if (fr.role != 3 && fr.role != 4) {
                continue;
            }
            painter.setBrush(pal.stringer);
            painter.setPen(cosmetic(pal.edge, 1.2));
            painter.drawRect(fr.rect);
        }
        int centerPieces = 0;
        for (const FrameRect& fr : model.frame()) {
            if (fr.role == 3) {
                ++centerPieces;
            }
        }
        drawLabel(painter, QPointF(spec.length * 0.5, spec.width + 3.4),
                  spec.doubleStringers
                      ? QStringLiteral("All tubes fit between leg faces    %1 in tube    %2 in off the floor")
                            .arg(QString::number(spec.stringerSize, 'f', 3), QString::number(spec.stringerHeight, 'f', 2))
                      : QStringLiteral("Center stringer in %1 pieces between the cross tubes    %2 in tube")
                            .arg(centerPieces)
                            .arg(QString::number(spec.stringerSize, 'f', 3)),
                  pal.muted, 13, Qt::AlignHCenter);
        double nestY = -kNestHeader;
        for (const TubeNest& nest : model.tubeNests()) {
            drawAnchoredLabel(painter, QPointF(0, nestY),
                              QStringLiteral("%1 in tube    %2 ft stock    kerf %3 in")
                                  .arg(QString::number(nest.size, 'f', 3))
                                  .arg(QString::number(nest.stockFeet, 'f', 0))
                                  .arg(QString::number(kTubeKerf, 'f', 3)),
                              pal.text, 12, Qt::AlignLeft | Qt::AlignBottom);
            nestY -= kNestHeader;
            for (size_t i = 0; i < nest.sticks.size(); ++i) {
                const TubeStick& stick = nest.sticks[i];
                nestY -= kNestBar;
                painter.setBrush(Qt::NoBrush);
                painter.setPen(cosmetic(pal.edge, 1.2));
                painter.drawRect(QRectF(0, nestY, stick.stockLength, kNestBar));
                double x = 0;
                for (const TubePiece& piece : stick.pieces) {
                    const bool legCut = piece.name.compare(QStringLiteral("Leg"), Qt::CaseInsensitive) == 0;
                    painter.setBrush(legCut ? pal.leg : pal.stringer);
                    painter.setPen(cosmetic(pal.edge, 1.0));
                    painter.drawRect(QRectF(x, nestY, piece.length, kNestBar));
                    x += piece.length + kTubeKerf;
                }
                drawNestPieceLabels(painter, stick, nestY, pal.text);
                const double drop = stick.stockLength - stick.used;
                drawAnchoredLabel(painter, QPointF(stick.stockLength + 2.0, nestY + kNestBar * 0.5),
                                  QStringLiteral("stick %1  drop %2 in").arg(i + 1).arg(QString::number(drop, 'f', 2)),
                                  pal.muted, 11, Qt::AlignLeft | Qt::AlignVCenter);
                nestY -= kNestBand;
            }
            nestY -= kNestGroupGap;
        }
    } else if (view == TableView::Iso) {
        paintSolid(painter, model, pal, yaw, pitch);
    }

    painter.restore();
    painter.restore();
}
