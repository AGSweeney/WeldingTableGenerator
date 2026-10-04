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

#include "TableModel.h"

#include <QtMath>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr double kPi = 3.14159265358979323846;

double polyArea(const std::vector<QPointF>& pts) {
    if (pts.size() < 3) {
        return 0;
    }
    double a = 0;
    const int n = static_cast<int>(pts.size());
    for (int i = 0; i < n; ++i) {
        const QPointF& p = pts[static_cast<size_t>(i)];
        const QPointF& q = pts[static_cast<size_t>((i + 1) % n)];
        a += p.x() * q.y() - q.x() * p.y();
    }
    return std::abs(a) * 0.5;
}

double capsuleArea(double length, double height) {
    const double r = height * 0.5;
    if (length <= height) {
        return kPi * r * r;
    }
    return (length - height) * height + kPi * r * r;
}

QRectF boundsOf(const std::vector<QPointF>& pts) {
    if (pts.empty()) {
        return {};
    }
    double x0 = pts[0].x();
    double y0 = pts[0].y();
    double x1 = x0;
    double y1 = y0;
    for (const QPointF& p : pts) {
        x0 = std::min(x0, p.x());
        y0 = std::min(y0, p.y());
        x1 = std::max(x1, p.x());
        y1 = std::max(y1, p.y());
    }
    return QRectF(QPointF(x0, y0), QPointF(x1, y1));
}

void addPt(std::vector<QPointF>& pts, double x, double y) {
    if (!pts.empty()) {
        const QPointF& q = pts.back();
        if (std::abs(q.x() - x) < 1e-9 && std::abs(q.y() - y) < 1e-9) {
            return;
        }
    }
    pts.emplace_back(x, y);
}

struct EdgeFeat {
    double x0 = 0;
    double x1 = 0;
    bool tab = false;
    double yInner = 0;
};

std::vector<double> gridCenters(double span, double margin, double pitch) {
    std::vector<double> v;
    if (pitch <= 1e-9 || span <= 0) {
        return v;
    }
    const double limit = span - margin;
    if (margin > limit + 1e-6) {
        return v;
    }
    int n = static_cast<int>(std::floor((limit - margin) / pitch + 1e-9)) + 1;
    if (n > 20000) {
        n = 20000;
    }
    v.reserve(static_cast<size_t>(std::max(0, n)));
    for (int i = 0; i < n; ++i) {
        const double x = margin + i * pitch;
        if (x > limit + 1e-6) {
            break;
        }
        v.push_back(x);
    }
    return v;
}

std::vector<double> midlines(const std::vector<double>& holes) {
    std::vector<double> m;
    if (holes.size() < 2) {
        return m;
    }
    m.reserve(holes.size() - 1);
    for (size_t i = 1; i < holes.size(); ++i) {
        m.push_back((holes[i - 1] + holes[i]) * 0.5);
    }
    return m;
}

int midlineStep(double spacing, double pitch) {
    if (pitch <= 1e-9) {
        return 2;
    }
    return std::max(2, static_cast<int>(std::lround(spacing / pitch)));
}

std::vector<double> apronSectionCenters(double span, const std::vector<double>& ribs) {
    std::vector<double> edges;
    edges.push_back(0.0);
    for (double rib : ribs) {
        if (rib > 1e-4 && rib < span - 1e-4) {
            edges.push_back(rib);
        }
    }
    edges.push_back(span);
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end(), [](double a, double b) { return std::abs(a - b) < 1e-6; }),
                edges.end());
    std::vector<double> centers;
    centers.reserve(edges.size());
    for (size_t i = 1; i < edges.size(); ++i) {
        centers.push_back((edges[i - 1] + edges[i]) * 0.5);
    }
    return centers;
}

void tabStations(const std::vector<double>& mids, double spacing, double pitch, std::vector<double>& tabs) {
    tabs.clear();
    if (mids.empty()) {
        return;
    }
    const int step = midlineStep(spacing, pitch);
    if (step == 2 && pitch > 1e-9 && spacing <= pitch * 1.5) {
        for (size_t i = 0; i < mids.size(); i += 2) {
            tabs.push_back(mids[i]);
        }
        return;
    }
    for (size_t i = 0; i < mids.size(); i += static_cast<size_t>(step)) {
        tabs.push_back(mids[i]);
    }
}

// Ribs start one midline in from the tab phase, the same distance from both ends.
// When that stride does not land evenly, the leftover bay stays in the center.
void centeredRibs(const std::vector<double>& mids, double span, double spacing, double pitch, std::vector<double>& ribs) {
    ribs.clear();
    const int n = static_cast<int>(mids.size());
    if (n <= 0 || span <= 0) {
        return;
    }
    if (n == 1) {
        ribs.push_back(mids[0]);
        return;
    }
    const int stride = midlineStep(spacing, pitch);
    const double target = mids[std::min(1, n - 1)];
    int bestI = -1;
    int bestJ = -1;
    double bestMismatch = 1e300;
    double bestInset = 1e300;
    for (int i = 0; i < n; ++i) {
        if (mids[static_cast<size_t>(i)] >= span * 0.5) {
            break;
        }
        for (int j = n - 1; j > i; --j) {
            if (mids[static_cast<size_t>(j)] <= span * 0.5) {
                break;
            }
            const double dL = mids[static_cast<size_t>(i)];
            const double dR = span - mids[static_cast<size_t>(j)];
            const double mismatch = std::abs(dL - dR);
            const double insetErr = std::abs(dL - target) + std::abs(dR - target);
            if (mismatch < bestMismatch - 1e-6 || (std::abs(mismatch - bestMismatch) <= 1e-6 && insetErr < bestInset - 1e-6)) {
                bestMismatch = mismatch;
                bestInset = insetErr;
                bestI = i;
                bestJ = j;
            }
        }
    }
    if (bestI < 0 || bestJ < 0) {
        ribs.push_back(mids[static_cast<size_t>(n / 2)]);
        return;
    }
    int i = bestI;
    int j = bestJ;
    while (i < j) {
        ribs.push_back(mids[static_cast<size_t>(i)]);
        ribs.push_back(mids[static_cast<size_t>(j)]);
        const int nextI = i + stride;
        const int nextJ = j - stride;
        if (nextI > nextJ) {
            break;
        }
        if (nextI == nextJ) {
            ribs.push_back(mids[static_cast<size_t>(nextI)]);
            break;
        }
        i = nextI;
        j = nextJ;
    }
    std::sort(ribs.begin(), ribs.end());
    ribs.erase(std::unique(ribs.begin(), ribs.end(), [](double a, double b) { return std::abs(a - b) < 1e-6; }), ribs.end());
}

// A rib and a tab cannot share one hole gap. The rib mirrored from the far end
// often lands on a tab, and deleting that tab leaves one end of the apron and
// the ribs without a tab. Move the tab into the nearest free gap instead.
void keepTabStations(const std::vector<double>& mids, std::vector<double>& tabs, std::vector<double>& ribs, double minSep,
                     double minRibGap, double low, double high, QStringList& errors, const QString& axis) {
    if (tabs.empty() || mids.empty()) {
        return;
    }
    const double center = (mids.front() + mids.back()) * 0.5;
    const int nTabs = static_cast<int>(tabs.size());
    const int nMids = static_cast<int>(mids.size());
    std::vector<int> order(static_cast<size_t>(nTabs));
    for (int i = 0; i < nTabs; ++i) {
        order[static_cast<size_t>(i)] = i;
    }
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return std::abs(tabs[static_cast<size_t>(a)] - center) > std::abs(tabs[static_cast<size_t>(b)] - center);
    });
    auto sharesRib = [&](double v) {
        for (double r : ribs) {
            if (std::abs(v - r) < 1e-6) {
                return true;
            }
        }
        return false;
    };
    std::vector<char> drop(static_cast<size_t>(nTabs), 0);
    auto tooClose = [&](double v, int self) {
        for (double r : ribs) {
            if (std::abs(v - r) < minRibGap) {
                return true;
            }
        }
        for (int i = 0; i < nTabs; ++i) {
            if (i == self || drop[static_cast<size_t>(i)]) {
                continue;
            }
            if (std::abs(tabs[static_cast<size_t>(i)] - v) < minSep) {
                return true;
            }
        }
        return false;
    };
    for (int ti : order) {
        if (!sharesRib(tabs[static_cast<size_t>(ti)])) {
            continue;
        }
        int from = -1;
        double bestD = 1e300;
        for (int i = 0; i < nMids; ++i) {
            const double d = std::abs(mids[static_cast<size_t>(i)] - tabs[static_cast<size_t>(ti)]);
            if (d < bestD) {
                bestD = d;
                from = i;
            }
        }
        if (bestD > 1e-4) {
            from = -1;
        }
        const bool preferHigh = tabs[static_cast<size_t>(ti)] >= center;
        int chosen = -1;
        for (int dist = 1; from >= 0 && dist < nMids && chosen < 0; ++dist) {
            const int candidates[2] = {preferHigh ? from + dist : from - dist, preferHigh ? from - dist : from + dist};
            for (int j : candidates) {
                if (j < 0 || j >= nMids) {
                    continue;
                }
                const double v = mids[static_cast<size_t>(j)];
                if (v < low - 1e-6 || v > high + 1e-6 || tooClose(v, ti)) {
                    continue;
                }
                chosen = j;
                break;
            }
        }
        if (chosen >= 0) {
            tabs[static_cast<size_t>(ti)] = mids[static_cast<size_t>(chosen)];
        } else {
            const double at = tabs[static_cast<size_t>(ti)];
            const auto before = ribs.size();
            ribs.erase(std::remove_if(ribs.begin(), ribs.end(),
                                      [at](double r) { return std::abs(r - at) < 1e-6; }),
                       ribs.end());
            if (ribs.size() == before) {
                drop[static_cast<size_t>(ti)] = 1;
                errors << QStringLiteral("A %1 tab at %2 in shares a hole gap with a rib, and no other gap can take it.")
                              .arg(axis, QString::number(at, 'f', 3));
            }
        }
    }
    std::vector<double> kept;
    kept.reserve(tabs.size());
    for (int i = 0; i < nTabs; ++i) {
        if (!drop[static_cast<size_t>(i)]) {
            kept.push_back(tabs[static_cast<size_t>(i)]);
        }
    }
    std::sort(kept.begin(), kept.end());
    kept.erase(std::unique(kept.begin(), kept.end(), [](double a, double b) { return std::abs(a - b) < 1e-6; }), kept.end());
    tabs.swap(kept);
}

void noteRibLayout(const std::vector<double>& ribs, const QString& name, QStringList& warnings) {
    if (ribs.size() < 3) {
        return;
    }
    const double mid = (ribs.front() + ribs.back()) * 0.5;
    double centerGap = ribs[1] - ribs[0];
    double centerAt = (ribs[1] + ribs[0]) * 0.5;
    double sideGap = 0;
    int sideCount = 0;
    for (size_t i = 1; i < ribs.size(); ++i) {
        const double gap = ribs[i] - ribs[i - 1];
        const double at = (ribs[i] + ribs[i - 1]) * 0.5;
        if (std::abs(at - mid) < std::abs(centerAt - mid)) {
            centerGap = gap;
            centerAt = at;
        }
    }
    for (size_t i = 1; i < ribs.size(); ++i) {
        const double gap = ribs[i] - ribs[i - 1];
        const double at = (ribs[i] + ribs[i - 1]) * 0.5;
        if (std::abs(at - mid) < 1e-6) {
            continue;
        }
        sideGap += gap;
        ++sideCount;
    }
    if (sideCount == 0) {
        return;
    }
    sideGap /= sideCount;
    if (std::abs(centerGap - sideGap) > 0.05) {
        warnings << QStringLiteral("%1 are equal from both ends. The middle bay is %2 in and the others are %3 in.")
                        .arg(name, QString::number(centerGap, 'f', 3), QString::number(sideGap, 'f', 3));
    }
}

std::vector<double> fitStations(const std::vector<double>& global, double origin, double length, double half,
                                const QString& name, QStringList& warnings) {
    std::vector<double> out;
    for (double g : global) {
        const double x = g - origin;
        if (x - half >= -1e-6 && x + half <= length + 1e-6) {
            out.push_back(x);
        } else {
            warnings << QStringLiteral("%1 at %2 in does not fit on the part and was skipped.")
                            .arg(name, QString::number(g, 'f', 3));
        }
    }
    return out;
}

struct EdgeNotch {
    int edge = 0;  // 0 bottom, 1 right, 2 top, 3 left
    double a = 0;
    double b = 0;
    double depth = 0;
};

std::vector<EdgeNotch> mergeNotches(const std::vector<EdgeNotch>& notches, int edge) {
    std::vector<EdgeNotch> v;
    for (const EdgeNotch& n : notches) {
        if (n.edge == edge && n.b - n.a > 1e-6 && n.depth > 1e-6) {
            v.push_back(n);
        }
    }
    std::sort(v.begin(), v.end(), [](const EdgeNotch& a, const EdgeNotch& b) { return a.a < b.a; });
    std::vector<EdgeNotch> merged;
    for (const EdgeNotch& n : v) {
        if (!merged.empty() && n.a <= merged.back().b + 1e-4) {
            merged.back().b = std::max(merged.back().b, n.b);
            merged.back().depth = std::max(merged.back().depth, n.depth);
        } else {
            merged.push_back(n);
        }
    }
    return merged;
}

std::vector<QPointF> plateWithNotches(double length, double width, const std::vector<EdgeNotch>& notches) {
    std::vector<QPointF> pts;
    addPt(pts, 0, 0);
    for (const EdgeNotch& n : mergeNotches(notches, 0)) {
        addPt(pts, n.a, 0);
        addPt(pts, n.a, n.depth);
        addPt(pts, n.b, n.depth);
        addPt(pts, n.b, 0);
    }
    addPt(pts, length, 0);
    for (const EdgeNotch& n : mergeNotches(notches, 1)) {
        addPt(pts, length, n.a);
        addPt(pts, length - n.depth, n.a);
        addPt(pts, length - n.depth, n.b);
        addPt(pts, length, n.b);
    }
    addPt(pts, length, width);
    auto top = mergeNotches(notches, 2);
    std::reverse(top.begin(), top.end());
    for (const EdgeNotch& n : top) {
        addPt(pts, n.b, width);
        addPt(pts, n.b, width - n.depth);
        addPt(pts, n.a, width - n.depth);
        addPt(pts, n.a, width);
    }
    addPt(pts, 0, width);
    auto left = mergeNotches(notches, 3);
    std::reverse(left.begin(), left.end());
    for (const EdgeNotch& n : left) {
        addPt(pts, 0, n.b);
        addPt(pts, n.depth, n.b);
        addPt(pts, n.depth, n.a);
        addPt(pts, 0, n.a);
    }
    addPt(pts, 0, 0);
    if (pts.size() >= 2) {
        const QPointF& a = pts.front();
        const QPointF& b = pts.back();
        if (std::abs(a.x() - b.x()) < 1e-9 && std::abs(a.y() - b.y()) < 1e-9) {
            pts.pop_back();
        }
    }
    return pts;
}

std::vector<QPointF> buildWeb(double length, double depth, const std::vector<EdgeFeat>& top,
                              const std::vector<EdgeFeat>& bottom, bool endTabs, double endLen, double endH) {
    std::vector<QPointF> pts;
    const double mid = depth * 0.5;
    const double y0 = mid - endH * 0.5;
    const double y1 = mid + endH * 0.5;

    addPt(pts, 0, 0);
    for (const EdgeFeat& f : top) {
        addPt(pts, f.x0, 0);
        addPt(pts, f.x0, f.yInner);
        addPt(pts, f.x1, f.yInner);
        addPt(pts, f.x1, 0);
    }
    addPt(pts, length, 0);
    if (endTabs) {
        addPt(pts, length, y0);
        addPt(pts, length + endLen, y0);
        addPt(pts, length + endLen, y1);
        addPt(pts, length, y1);
    }
    addPt(pts, length, depth);
    for (const EdgeFeat& f : bottom) {
        addPt(pts, f.x1, depth);
        addPt(pts, f.x1, f.yInner);
        addPt(pts, f.x0, f.yInner);
        addPt(pts, f.x0, depth);
    }
    addPt(pts, 0, depth);
    if (endTabs) {
        addPt(pts, 0, y1);
        addPt(pts, -endLen, y1);
        addPt(pts, -endLen, y0);
        addPt(pts, 0, y0);
    }
    addPt(pts, 0, 0);
    if (pts.size() >= 2) {
        const QPointF& a = pts.front();
        const QPointF& b = pts.back();
        if (std::abs(a.x() - b.x()) < 1e-9 && std::abs(a.y() - b.y()) < 1e-9) {
            pts.pop_back();
        }
    }
    return pts;
}

void addOpenings(Part& part, const std::vector<double>& edges, const TableSpec& spec) {
    if (!spec.lightening || edges.size() < 2) {
        return;
    }
    const double y = spec.webDepth * 0.5;
    for (size_t i = 1; i < edges.size(); ++i) {
        const double a = edges[i - 1];
        const double b = edges[i];
        const double bay = b - a;
        if (bay <= 0) {
            continue;
        }
        const double land = bay >= spec.landThreshold ? spec.landWide : spec.landNarrow;
        const double usable = bay - 2.0 * land;
        if (usable + 1e-9 < spec.openingMin || spec.openingMax <= 1e-9) {
            continue;
        }
        int n = 1;
        while ((n + 1) * spec.openingMax + n * spec.openingGap <= usable + 1e-9 && n < 12) {
            ++n;
        }
        double length = std::min(spec.openingMax, (usable - (n - 1) * spec.openingGap) / n);
        if (spec.openingSnap > 1e-9) {
            length = std::floor(length / spec.openingSnap + 1e-9) * spec.openingSnap;
        }
        if (length + 1e-9 < spec.openingMin) {
            continue;
        }
        if (length + 1e-9 < spec.openingHeight) {
            length = spec.openingHeight;
            if (length > usable + 1e-9) {
                continue;
            }
        }
        const double group = n * length + (n - 1) * spec.openingGap;
        double start = a + (bay - group) * 0.5;
        for (int k = 0; k < n; ++k) {
            const double cx = start + length * 0.5;
            part.caps.push_back({cx, y, length, spec.openingHeight});
            start += length + spec.openingGap;
        }
    }
}

void finishPart(Part& part, double density) {
    part.bounds = boundsOf(part.outer);
    double area = polyArea(part.outer);
    for (const CircleFeat& h : part.holes) {
        area -= kPi * h.r * h.r;
    }
    for (const SlotFeat& s : part.slotCuts) {
        area -= std::abs((s.x1 - s.x0) * (s.y1 - s.y0));
    }
    for (const CapsuleFeat& c : part.caps) {
        area -= capsuleArea(c.length, c.height);
    }
    part.area = std::max(0.0, area);
    part.weight = part.area * part.thickness * density;
}

struct TubeBlank {
    QString name;
    double length = 0;
    double size = 0;
};

bool packTubeSticks(const std::vector<TubeBlank>& cuts, double stock, std::vector<TubeStick>& sticks) {
    sticks.clear();
    std::vector<TubeBlank> ordered = cuts;
    std::sort(ordered.begin(), ordered.end(), [](const TubeBlank& a, const TubeBlank& b) { return a.length > b.length; });
    for (const TubeBlank& cut : ordered) {
        if (cut.length > stock + 1e-6) {
            sticks.clear();
            return false;
        }
        bool placed = false;
        for (TubeStick& stick : sticks) {
            const double kerf = stick.pieces.empty() ? 0.0 : kTubeKerf;
            if (stick.used + kerf + cut.length <= stock + 1e-6) {
                stick.used += kerf + cut.length;
                stick.pieces.push_back({cut.name, cut.length});
                placed = true;
                break;
            }
        }
        if (!placed) {
            TubeStick stick;
            stick.size = cut.size;
            stick.stockLength = stock;
            stick.stockFeet = stock / 12.0;
            stick.used = cut.length;
            stick.pieces.push_back({cut.name, cut.length});
            sticks.push_back(std::move(stick));
        }
    }
    return true;
}

bool rectContains(const SlotFeat& outer, double x0, double y0, double x1, double y1) {
    return outer.x0 <= x0 + 1e-6 && outer.y0 <= y0 + 1e-6 && outer.x1 + 1e-6 >= x1 && outer.y1 + 1e-6 >= y1;
}

}  // namespace

TableSpec TableSpec::revA() { return {}; }

QJsonObject TableSpec::toJson() const {
    QJsonObject o;
    o.insert(QStringLiteral("length"), length);
    o.insert(QStringLiteral("width"), width);
    o.insert(QStringLiteral("topThickness"), topThickness);
    o.insert(QStringLiteral("webThickness"), webThickness);
    o.insert(QStringLiteral("webDepth"), webDepth);
    o.insert(QStringLiteral("holeDiameterMm"), holeDiameterMm);
    o.insert(QStringLiteral("holeDiameterInch"), holeDiameterInch);
    o.insert(QStringLiteral("holePitchX"), holePitchX);
    o.insert(QStringLiteral("holePitchY"), holePitchY);
    o.insert(QStringLiteral("holeMarginX"), holeMarginX);
    o.insert(QStringLiteral("holeMarginY"), holeMarginY);
    o.insert(QStringLiteral("apronInset"), apronInset);
    o.insert(QStringLiteral("slotClearance"), slotClearance);
    o.insert(QStringLiteral("tabWidth"), tabWidth);
    o.insert(QStringLiteral("slotExtra"), slotExtra);
    o.insert(QStringLiteral("tabHeight"), tabHeight);
    o.insert(QStringLiteral("endTabLength"), endTabLength);
    o.insert(QStringLiteral("endTabHeight"), endTabHeight);
    o.insert(QStringLiteral("ribEndGap"), ribEndGap);
    o.insert(QStringLiteral("crossRibSpacing"), crossRibSpacing);
    o.insert(QStringLiteral("longRibSpacing"), longRibSpacing);
    o.insert(QStringLiteral("apronTopSlots"), apronTopSlots);
    o.insert(QStringLiteral("apronHoles"), apronHoles);
    o.insert(QStringLiteral("apronHoleFromTop"), apronHoleFromTop);
    o.insert(QStringLiteral("apronHoleFromBottom"), apronHoleFromBottom);
    o.insert(QStringLiteral("halfLapExtra"), halfLapExtra);
    o.insert(QStringLiteral("lightening"), lightening);
    o.insert(QStringLiteral("openingHeight"), openingHeight);
    o.insert(QStringLiteral("landWide"), landWide);
    o.insert(QStringLiteral("landNarrow"), landNarrow);
    o.insert(QStringLiteral("landThreshold"), landThreshold);
    o.insert(QStringLiteral("openingGap"), openingGap);
    o.insert(QStringLiteral("openingMax"), openingMax);
    o.insert(QStringLiteral("openingMin"), openingMin);
    o.insert(QStringLiteral("openingSnap"), openingSnap);
    o.insert(QStringLiteral("frame"), frame);
    o.insert(QStringLiteral("tubeSize"), tubeSize);
    o.insert(QStringLiteral("stringerSize"), stringerSize);
    o.insert(QStringLiteral("stringerHeight"), stringerHeight);
    o.insert(QStringLiteral("doubleStringers"), doubleStringers);
    o.insert(QStringLiteral("tubeWall"), tubeWall);
    o.insert(QStringLiteral("frameInsetX"), frameInsetX);
    o.insert(QStringLiteral("frameInsetY"), frameInsetY);
    o.insert(QStringLiteral("frameSupports"), frameSupports);
    o.insert(QStringLiteral("finishedHeight"), finishedHeight);
    o.insert(QStringLiteral("footExtension"), footExtension);
    o.insert(QStringLiteral("sheetLength"), sheetLength);
    o.insert(QStringLiteral("sheetWidth"), sheetWidth);
    o.insert(QStringLiteral("nestGap"), nestGap);
    o.insert(QStringLiteral("nestMargin"), nestMargin);
    o.insert(QStringLiteral("nestCoupon"), nestCoupon);
    o.insert(QStringLiteral("includeQ02"), includeQ02);
    o.insert(QStringLiteral("density"), density);
    o.insert(QStringLiteral("revision"), revision);
    o.insert(QStringLiteral("title"), title);
    o.insert(QStringLiteral("shopNotes"), shopNotes);
    o.insert(QStringLiteral("writeIndividuals"), writeIndividuals);
    o.insert(QStringLiteral("writeNest"), writeNest);
    o.insert(QStringLiteral("writeFrame"), writeFrame);
    o.insert(QStringLiteral("writePdf"), writePdf);
    o.insert(QStringLiteral("writeReadme"), writeReadme);
    o.insert(QStringLiteral("writeJson"), writeJson);
    return o;
}

TableSpec TableSpec::fromJson(const QJsonObject& o, const TableSpec& fallback) {
    TableSpec s = fallback;
    auto d = [&](const char* k, double& dst) {
        if (o.contains(QLatin1String(k))) {
            dst = o.value(QLatin1String(k)).toDouble(dst);
        }
    };
    auto b = [&](const char* k, bool& dst) {
        if (o.contains(QLatin1String(k))) {
            dst = o.value(QLatin1String(k)).toBool(dst);
        }
    };
    d("length", s.length);
    d("width", s.width);
    d("topThickness", s.topThickness);
    d("webThickness", s.webThickness);
    d("webDepth", s.webDepth);
    d("holeDiameterMm", s.holeDiameterMm);
    b("holeDiameterInch", s.holeDiameterInch);
    d("holePitchX", s.holePitchX);
    d("holePitchY", s.holePitchY);
    d("holeMarginX", s.holeMarginX);
    d("holeMarginY", s.holeMarginY);
    d("apronInset", s.apronInset);
    d("slotClearance", s.slotClearance);
    d("tabWidth", s.tabWidth);
    d("slotExtra", s.slotExtra);
    d("tabHeight", s.tabHeight);
    d("endTabLength", s.endTabLength);
    d("endTabHeight", s.endTabHeight);
    d("ribEndGap", s.ribEndGap);
    d("crossRibSpacing", s.crossRibSpacing);
    d("longRibSpacing", s.longRibSpacing);
    b("apronTopSlots", s.apronTopSlots);
    b("apronHoles", s.apronHoles);
    d("apronHoleFromTop", s.apronHoleFromTop);
    d("apronHoleFromBottom", s.apronHoleFromBottom);
    d("halfLapExtra", s.halfLapExtra);
    b("lightening", s.lightening);
    d("openingHeight", s.openingHeight);
    d("landWide", s.landWide);
    d("landNarrow", s.landNarrow);
    d("landThreshold", s.landThreshold);
    d("openingGap", s.openingGap);
    d("openingMax", s.openingMax);
    d("openingMin", s.openingMin);
    d("openingSnap", s.openingSnap);
    b("frame", s.frame);
    d("tubeSize", s.tubeSize);
    d("stringerSize", s.stringerSize);
    d("stringerHeight", s.stringerHeight);
    b("doubleStringers", s.doubleStringers);
    d("tubeWall", s.tubeWall);
    d("frameInsetX", s.frameInsetX);
    d("frameInsetY", s.frameInsetY);
    if (o.contains(QStringLiteral("frameSupports"))) {
        s.frameSupports = o.value(QStringLiteral("frameSupports")).toInt(s.frameSupports);
    }
    d("finishedHeight", s.finishedHeight);
    d("footExtension", s.footExtension);
    d("sheetLength", s.sheetLength);
    d("sheetWidth", s.sheetWidth);
    d("nestGap", s.nestGap);
    d("nestMargin", s.nestMargin);
    b("nestCoupon", s.nestCoupon);
    b("includeQ02", s.includeQ02);
    d("density", s.density);
    if (o.contains(QStringLiteral("revision"))) {
        s.revision = o.value(QStringLiteral("revision")).toString(s.revision);
    }
    if (o.contains(QStringLiteral("title"))) {
        s.title = o.value(QStringLiteral("title")).toString(s.title);
    }
    if (o.contains(QStringLiteral("shopNotes"))) {
        s.shopNotes = o.value(QStringLiteral("shopNotes")).toString(s.shopNotes);
    }
    b("writeIndividuals", s.writeIndividuals);
    b("writeNest", s.writeNest);
    b("writeFrame", s.writeFrame);
    b("writePdf", s.writePdf);
    b("writeReadme", s.writeReadme);
    b("writeJson", s.writeJson);
    return s;
}

const Part* TableModel::find(const QString& code) const {
    for (const Part& p : m_parts) {
        if (p.code == code) {
            return &p;
        }
    }
    return nullptr;
}

int TableModel::topHoleCount() const {
    const Part* p = find(QStringLiteral("P01"));
    return p ? static_cast<int>(p->holes.size()) : 0;
}

int TableModel::topSlotCount() const {
    const Part* p = find(QStringLiteral("P01"));
    return p ? static_cast<int>(p->slotCuts.size()) + p->openEdgeSlots : 0;
}

double TableModel::slotWidth() const { return m_spec.webThickness + m_spec.slotClearance; }
double TableModel::topSlotLength() const { return m_spec.tabWidth + m_spec.slotExtra; }
double TableModel::apronSlotLength() const { return m_spec.endTabHeight + m_spec.slotExtra; }
double TableModel::holeDiameterIn() const { return m_spec.holeDiameterMm / 25.4; }

QString TableModel::dogHoleLabel() const {
    if (m_spec.holeDiameterInch) {
        return QStringLiteral("%1 in (%2 mm)")
            .arg(QString::number(holeDiameterIn(), 'f', 4), QString::number(m_spec.holeDiameterMm, 'f', 3));
    }
    return QStringLiteral("%1 mm (%2 in)")
        .arg(QString::number(m_spec.holeDiameterMm, 'f', 3), QString::number(holeDiameterIn(), 'f', 4));
}

double TableModel::legLengthExample() const {
    return m_spec.finishedHeight - m_spec.topThickness - footPlateThickness();
}

QString TableModel::summary() const {
    if (!m_errors.isEmpty()) {
        return m_errors.first();
    }
    const QString nest = !m_nestOk ? QStringLiteral("nest does not fit")
                                   : (m_nestSheets <= 1 ? QStringLiteral("nest fits")
                                                       : QStringLiteral("%1 sheets").arg(m_nestSheets));
    return QStringLiteral("%1 x %2 in  ·  %3 holes  ·  %4 slots  ·  %5 long ribs  ·  %6 cross ribs  ·  %7  ·  %8 lb")
        .arg(QString::number(m_spec.width, 'f', 3), QString::number(m_spec.length, 'f', 3))
        .arg(topHoleCount())
        .arg(topSlotCount())
        .arg(longRibCount())
        .arg(crossRibCount())
        .arg(nest, QString::number(m_assemblyWeight, 'f', 1));
}

namespace {

double distToSegment(double x, double y, const QPointF& a, const QPointF& b) {
    const double dx = b.x() - a.x();
    const double dy = b.y() - a.y();
    const double len2 = dx * dx + dy * dy;
    double t = 0;
    if (len2 > 1e-18) {
        t = std::clamp(((x - a.x()) * dx + (y - a.y()) * dy) / len2, 0.0, 1.0);
    }
    return std::hypot(x - (a.x() + t * dx), y - (a.y() + t * dy));
}

bool pointInPolygon(double x, double y, const std::vector<QPointF>& poly) {
    bool inside = false;
    const int n = static_cast<int>(poly.size());
    for (int i = 0, j = n - 1; i < n; j = i++) {
        const double xi = poly[static_cast<size_t>(i)].x();
        const double yi = poly[static_cast<size_t>(i)].y();
        const double xj = poly[static_cast<size_t>(j)].x();
        const double yj = poly[static_cast<size_t>(j)].y();
        const bool cross = (yi > y) != (yj > y) && (x < (xj - xi) * (y - yi) / ((yj - yi) + 0.0) + xi);
        if (cross) {
            inside = !inside;
        }
    }
    return inside;
}

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

bool contourCrossesItself(const std::vector<QPointF>& pts) {
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
            if (segmentsCross(a, b, pts[static_cast<size_t>(j)], pts[static_cast<size_t>((j + 1) % n)])) {
                return true;
            }
        }
    }
    return false;
}

double distToBox(double x, double y, double x0, double y0, double x1, double y1) {
    const double dx = x < x0 ? x0 - x : (x > x1 ? x - x1 : 0.0);
    const double dy = y < y0 ? y0 - y : (y > y1 ? y - y1 : 0.0);
    return std::hypot(dx, dy);
}

void noteCutGeometry(const TableSpec& spec, const std::vector<Part>& parts, const std::vector<double>& longY,
                     const std::vector<double>& crossX, double apronInner, double ribOriginX, double longRibLen,
                     double ribOriginY, double crossRibLen, QStringList& errors, QStringList& warnings, int& clampBlocked) {
    constexpr double kClampClearance = 1.0;
    int extra = 0;
    auto addError = [&](const QString& message) {
        if (extra < 8) {
            errors << message;
        }
        ++extra;
    };
    for (const Part& part : parts) {
        if (part.qty < 1) {
            continue;
        }
        if (part.outer.size() >= 4 && contourCrossesItself(part.outer)) {
            addError(QStringLiteral("%1 outline crosses itself.").arg(part.code));
        }
        for (size_t i = 0; i < part.holes.size(); ++i) {
            const CircleFeat& hole = part.holes[i];
            if (part.outer.size() >= 3) {
                const bool inside = pointInPolygon(hole.x, hole.y, part.outer);
                double edge = 1e9;
                const int n = static_cast<int>(part.outer.size());
                for (int k = 0; k < n; ++k) {
                    edge = std::min(edge, distToSegment(hole.x, hole.y, part.outer[static_cast<size_t>(k)],
                                                        part.outer[static_cast<size_t>((k + 1) % n)]));
                }
                if (!inside || edge < hole.r - 1e-4) {
                    addError(QStringLiteral("%1 hole at %2, %3 breaks through the edge.")
                                 .arg(part.code)
                                 .arg(QString::number(hole.x, 'f', 3), QString::number(hole.y, 'f', 3)));
                }
            }
            for (size_t j = i + 1; j < part.holes.size(); ++j) {
                const CircleFeat& other = part.holes[j];
                if (std::hypot(hole.x - other.x, hole.y - other.y) < hole.r + other.r - 1e-4) {
                    addError(QStringLiteral("%1 holes overlap. Increase pitch or reduce the diameter.").arg(part.code));
                    break;
                }
            }
            for (const SlotFeat& slot : part.slotCuts) {
                const double x0 = std::min(slot.x0, slot.x1);
                const double x1 = std::max(slot.x0, slot.x1);
                const double y0 = std::min(slot.y0, slot.y1);
                const double y1 = std::max(slot.y0, slot.y1);
                if (distToBox(hole.x, hole.y, x0, y0, x1, y1) < hole.r - 1e-4) {
                    addError(QStringLiteral("%1 hole at %2, %3 intersects a slot.")
                                 .arg(part.code)
                                 .arg(QString::number(hole.x, 'f', 3), QString::number(hole.y, 'f', 3)));
                    break;
                }
            }
        }
    }
    if (extra > 8) {
        errors << QStringLiteral("%1 more geometry problems were found.").arg(extra - 8);
    }

    const Part* top = nullptr;
    for (const Part& part : parts) {
        if (part.code == QLatin1String("P01")) {
            top = &part;
            break;
        }
    }
    if (!top) {
        return;
    }
    struct Box {
        double x0, y0, x1, y1;
    };
    std::vector<Box> webs;
    const double web = spec.webThickness;
    const double inset = spec.apronInset;
    webs.push_back({inset, inset, spec.length - inset, inset + web});
    webs.push_back({inset, spec.width - inset - web, spec.length - inset, spec.width - inset});
    webs.push_back({inset, apronInner, inset + web, spec.width - apronInner});
    webs.push_back({spec.length - inset - web, apronInner, spec.length - inset, spec.width - apronInner});
    for (double y : longY) {
        webs.push_back({ribOriginX, y - web * 0.5, ribOriginX + longRibLen, y + web * 0.5});
    }
    for (double x : crossX) {
        webs.push_back({x - web * 0.5, ribOriginY, x + web * 0.5, ribOriginY + crossRibLen});
    }
    clampBlocked = 0;
    for (const CircleFeat& hole : top->holes) {
        for (const Box& box : webs) {
            if (distToBox(hole.x, hole.y, box.x0, box.y0, box.x1, box.y1) < hole.r + kClampClearance) {
                ++clampBlocked;
                break;
            }
        }
    }
    if (clampBlocked > 0) {
        warnings << QStringLiteral("%1 dog holes have less than %2 in between the hole edge and a rib or apron. "
                                   "A clamp can be blocked under a hole that still accepts a pin.")
                        .arg(clampBlocked)
                        .arg(QString::number(kClampClearance, 'f', 3));
    }
}

}  // namespace

bool TableModel::rebuild(const TableSpec& spec) {
    m_spec = spec;
    m_errors.clear();
    m_warnings.clear();
    m_checks.clear();
    m_parts.clear();
    m_nest.clear();
    m_frame.clear();
    m_tubeNests.clear();
    m_nestOk = false;
    m_nestSheets = 0;
    m_nestError.clear();
    m_nestOccupied = {};
    m_assemblyWeight = 0;
    m_lighteningSaved = 0;
    m_clampBlocked = 0;
    m_apronTabX.clear();
    m_apronTabY.clear();
    m_tabHoleHits.clear();

    auto need = [&](bool ok, const QString& msg) {
        if (!ok) {
            m_errors << msg;
        }
    };
    need(spec.length >= 8.0 && spec.width >= 8.0, QStringLiteral("Plate length and width must be at least 8 in."));
    need(spec.topThickness > 0.05 && spec.webThickness > 0.02, QStringLiteral("Thickness must be positive."));
    need(spec.webDepth >= 3.0, QStringLiteral("Web depth must be at least 3 in so apron holes and end tabs fit."));
    need(spec.tabHeight > 0 && spec.tabHeight + 0.02 < spec.topThickness,
         QStringLiteral("Top tab height must be shorter than the top thickness so the working face stays recessed."));
    need(spec.endTabHeight > 0.2 && spec.endTabHeight + 0.2 < spec.webDepth,
         QStringLiteral("End tab height must sit inside the web depth."));
    need(spec.holePitchX > 0.2 && spec.holePitchY > 0.2, QStringLiteral("Hole pitch must be positive."));
    need(spec.holeDiameterMm > 1.0, QStringLiteral("Hole diameter must be positive."));
    need(spec.tabWidth > 0.2 && spec.slotExtra >= 0 && spec.slotClearance >= 0,
         QStringLiteral("Tab width and clearances must be non-negative."));
    need(spec.apronInset >= 0 && spec.apronInset + spec.webThickness * 2 < std::min(spec.length, spec.width) * 0.5,
         QStringLiteral("Apron inset leaves no room for the webs."));
    need(spec.sheetLength > 4 && spec.sheetWidth > 4, QStringLiteral("Sheet size must be positive."));
    need(spec.nestGap >= 0 && spec.nestMargin >= 0, QStringLiteral("Nest gap and margin must be non-negative."));
    need(!spec.lightening || (spec.openingHeight > 0.4 && spec.openingHeight + 0.2 < spec.webDepth),
         QStringLiteral("Lightening opening must sit inside the web with material above and below."));
    need(spec.frameSupports >= 2, QStringLiteral("Use at least two leg pairs."));
    need(spec.tubeSize >= 0.75, QStringLiteral("Leg size must be at least 0.75 in."));
    need(spec.stringerSize >= 0.5 && spec.stringerSize <= spec.tubeSize + 1e-9,
         QStringLiteral("Stringer size must be at least 0.5 in and no larger than the leg."));
    need(spec.stringerHeight + 1e-6 >= spec.topThickness,
         QStringLiteral("Raise the stringers so they sit above the foot plate."));
    need(spec.stringerHeight + spec.stringerSize <= spec.finishedHeight - spec.topThickness + 1e-6,
         QStringLiteral("Stringers must stay on the leg, below the underside of the top."));
    need(!spec.frame || spec.finishedHeight >= spec.topThickness + spec.webDepth + spec.topThickness + 1.0,
         QStringLiteral("Finished height must clear the top, the apron, and the foot plate."));
    {
        const double inside = spec.apronInset + spec.webThickness + spec.tubeSize;
        need(spec.length > inside * 2.0 + 1.0 && spec.width > inside * 2.0 + 1.0,
             QStringLiteral("Leg size does not fit inside the apron."));
    }
    if (!m_errors.isEmpty()) {
        return false;
    }

    const double holeR = holeDiameterIn() * 0.5;
    const double sw = slotWidth();
    const double slTop = topSlotLength();
    const double slApron = apronSlotLength();
    const double halfTab = spec.tabWidth * 0.5;
    const double halfLap = sw * 0.5;

    m_holeX = gridCenters(spec.length, spec.holeMarginX, spec.holePitchX);
    m_holeY = gridCenters(spec.width, spec.holeMarginY, spec.holePitchY);
    if (m_holeX.empty() || m_holeY.empty()) {
        m_errors << QStringLiteral("The hole grid does not fit inside the margins.");
        return false;
    }
    const auto midsX = midlines(m_holeX);
    const auto midsY = midlines(m_holeY);
    tabStations(midsX, spec.crossRibSpacing, spec.holePitchX, m_tabX);
    tabStations(midsY, spec.longRibSpacing, spec.holePitchY, m_tabY);
    centeredRibs(midsX, spec.length, spec.crossRibSpacing, spec.holePitchX, m_crossX);
    centeredRibs(midsY, spec.width, spec.longRibSpacing, spec.holePitchY, m_longY);
    const double apronInner = spec.apronInset + spec.webThickness;
    const double minSep = slTop;
    const double minRibGap = slTop * 0.5 + sw * 0.5;
    keepTabStations(midsX, m_tabX, m_crossX, minSep, minRibGap, apronInner + spec.ribEndGap + halfTab,
                    spec.length - spec.apronInset - spec.webThickness - spec.ribEndGap - halfTab, m_errors,
                    QStringLiteral("length"));
    keepTabStations(midsY, m_tabY, m_longY, minSep, minRibGap, apronInner + spec.ribEndGap + halfTab,
                    spec.width - apronInner - spec.ribEndGap - halfTab, m_errors, QStringLiteral("width"));
    m_apronTabX = apronSectionCenters(spec.length, m_crossX);
    m_apronTabY = apronSectionCenters(spec.width, m_longY);
    if (m_tabX.empty() || m_tabY.empty()) {
        m_errors << QStringLiteral("Not enough hole rows to place tab slots between holes. Add margin room or reduce pitch.");
    }
    noteRibLayout(m_crossX, QStringLiteral("Cross ribs"), m_warnings);
    noteRibLayout(m_longY, QStringLiteral("Long ribs"), m_warnings);
    const double actualCross = midlineStep(spec.crossRibSpacing, spec.holePitchX) * spec.holePitchX;
    if (std::abs(actualCross - spec.crossRibSpacing) > 0.05) {
        m_warnings << QStringLiteral("Cross ribs sit on the hole midlines, so the regular bay is %1 in rather than %2 in.")
                          .arg(QString::number(actualCross, 'f', 3), QString::number(spec.crossRibSpacing, 'f', 3));
    }

    const double A0 = spec.apronInset;
    m_apronInner = spec.apronInset + spec.webThickness;
    m_longApronLen = spec.length - 2.0 * spec.apronInset;
    m_endApronLen = spec.width - 2.0 * m_apronInner;
    m_r0 = m_apronInner + spec.ribEndGap;
    const double r1 = spec.length - spec.apronInset - spec.webThickness - spec.ribEndGap;
    m_longRibLen = r1 - m_r0;
    m_c0 = m_apronInner + spec.ribEndGap;
    const double c1 = spec.width - m_apronInner - spec.ribEndGap;
    m_crossRibLen = c1 - m_c0;
    if (m_longApronLen < 4 || m_endApronLen < 4 || m_longRibLen < 4 || m_crossRibLen < 4) {
        m_errors << QStringLiteral("Aprons or ribs are shorter than 4 in. Reduce inset, thickness, or gap.");
        return false;
    }

    const double mid = spec.webDepth * 0.5;
    const double topNotch = mid + spec.halfLapExtra;
    const double botNotch = mid - spec.halfLapExtra;

    auto features = [&](const std::vector<double>& locals, bool tabs, double yInner, double half) {
        std::vector<EdgeFeat> feats;
        for (double x : locals) {
            EdgeFeat f;
            f.x0 = x - half;
            f.x1 = x + half;
            f.tab = tabs;
            f.yInner = yInner;
            if (f.x0 < -1e-6 || f.x1 > 1e9) {
                continue;
            }
            feats.push_back(f);
        }
        std::sort(feats.begin(), feats.end(), [](const EdgeFeat& a, const EdgeFeat& b) { return a.x0 < b.x0; });
        return feats;
    };

    auto makeWeb = [&](const QString& code, const QString& name, int qty, double length, bool endTabs,
                       const std::vector<double>& tabLocals, const std::vector<double>& lapLocals, bool lapFromTop,
                       const std::vector<CircleFeat>& holes, const std::vector<SlotFeat>& receivers,
                       const std::vector<double>& openingEdges) {
        Part part;
        part.code = code;
        part.name = name;
        part.thickness = spec.webThickness;
        part.qty = qty;
        part.bodyLength = length;
        auto top = features(tabLocals, true, -spec.tabHeight, halfTab);
        std::vector<EdgeFeat> laps = features(lapLocals, false, lapFromTop ? topNotch : botNotch, halfLap);
        std::vector<EdgeFeat> bot;
        if (!lapFromTop) {
            bot = laps;
            std::sort(bot.begin(), bot.end(), [](const EdgeFeat& a, const EdgeFeat& b) { return a.x0 > b.x0; });
            laps.clear();
        }
        part.outer = buildWeb(length, spec.webDepth, lapFromTop ? top : top, bot, endTabs, spec.endTabLength,
                              spec.endTabHeight);
        if (lapFromTop) {
            // Rebuild with top notches merged into the tab edge. Tabs and notches share the top edge.
            std::vector<EdgeFeat> merged = top;
            merged.insert(merged.end(), laps.begin(), laps.end());
            std::sort(merged.begin(), merged.end(), [](const EdgeFeat& a, const EdgeFeat& b) { return a.x0 < b.x0; });
            part.outer = buildWeb(length, spec.webDepth, merged, {}, endTabs, spec.endTabLength, spec.endTabHeight);
        }
        part.holes = holes;
        part.slotCuts = receivers;
        addOpenings(part, openingEdges, spec);
        finishPart(part, spec.density);
        m_parts.push_back(std::move(part));
    };

    // Top. Slots that break the plate edge, which happens when the apron is flush,
    // are opened through that edge instead of being dropped.
    std::vector<SlotFeat> topCovers;
    {
        Part top;
        top.code = QStringLiteral("P01");
        top.name = QStringLiteral("Top");
        top.thickness = spec.topThickness;
        top.qty = 1;
        top.bodyLength = spec.length;
        std::vector<EdgeNotch> notches;
        for (double x : m_holeX) {
            for (double y : m_holeY) {
                top.holes.push_back({x, y, holeR});
            }
        }
        const double apronY0 = A0 + spec.webThickness * 0.5;
        const double apronY1 = spec.width - A0 - spec.webThickness * 0.5;
        const double apronX0 = A0 + spec.webThickness * 0.5;
        const double apronX1 = spec.length - A0 - spec.webThickness * 0.5;
        auto placeSlot = [&](const SlotFeat& s) {
            const bool outL = s.x0 < -1e-6;
            const bool outR = s.x1 > spec.length + 1e-6;
            const bool outB = s.y0 < -1e-6;
            const bool outT = s.y1 > spec.width + 1e-6;
            const int outside = static_cast<int>(outL) + static_cast<int>(outR) + static_cast<int>(outB) + static_cast<int>(outT);
            if (outside == 0) {
                top.slotCuts.push_back(s);
                topCovers.push_back(s);
                return;
            }
            auto notch = [&](int edge, double a, double b, double depth) {
                const double limit = (edge == 0 || edge == 2) ? spec.length : spec.width;
                const double span = (edge == 0 || edge == 2) ? spec.width : spec.length;
                a = std::max(0.0, a);
                b = std::min(limit, b);
                depth = std::min(depth, span * 0.5);
                if (b - a > 1e-4 && depth > 1e-4) {
                    notches.push_back({edge, a, b, depth});
                    topCovers.push_back(s);
                    ++top.openEdgeSlots;
                }
            };
            if (outside == 1 && outB && s.y1 > 1e-4) {
                notch(0, s.x0, s.x1, s.y1);
            } else if (outside == 1 && outT && s.y0 < spec.width - 1e-4) {
                notch(2, s.x0, s.x1, spec.width - s.y0);
            } else if (outside == 1 && outL && s.x1 > 1e-4) {
                notch(3, s.y0, s.y1, s.x1);
            } else if (outside == 1 && outR && s.x0 < spec.length - 1e-4) {
                notch(1, s.y0, s.y1, spec.length - s.x0);
            } else {
                m_errors << QStringLiteral("A top slot at X %1 Y %2 falls off the plate and was skipped.")
                                  .arg(QString::number((s.x0 + s.x1) * 0.5, 'f', 3),
                                       QString::number((s.y0 + s.y1) * 0.5, 'f', 3));
            }
        };
        auto placeApronSlot = [&](const SlotFeat& s) {
            placeSlot(s);
            const double x0 = std::min(s.x0, s.x1);
            const double x1 = std::max(s.x0, s.x1);
            const double y0 = std::min(s.y0, s.y1);
            const double y1 = std::max(s.y0, s.y1);
            for (const CircleFeat& hole : top.holes) {
                if (distToBox(hole.x, hole.y, x0, y0, x1, y1) < hole.r - 1e-4) {
                    m_tabHoleHits.push_back(s);
                    if (m_tabHoleHits.size() <= 8) {
                        m_errors << QStringLiteral("Apron tab at X %1 Y %2 cuts a dog hole.")
                                        .arg(QString::number((s.x0 + s.x1) * 0.5, 'f', 3),
                                             QString::number((s.y0 + s.y1) * 0.5, 'f', 3));
                    }
                    return;
                }
            }
        };
        if (spec.apronTopSlots) {
            for (double y : {apronY0, apronY1}) {
                for (double x : m_apronTabX) {
                    placeApronSlot(SlotFeat{x - slTop * 0.5, y - sw * 0.5, x + slTop * 0.5, y + sw * 0.5});
                }
            }
            for (double x : {apronX0, apronX1}) {
                for (double y : m_apronTabY) {
                    placeApronSlot(SlotFeat{x - sw * 0.5, y - slTop * 0.5, x + sw * 0.5, y + slTop * 0.5});
                }
            }
        }
        for (double y : m_longY) {
            for (double x : m_tabX) {
                placeSlot(SlotFeat{x - slTop * 0.5, y - sw * 0.5, x + slTop * 0.5, y + sw * 0.5});
            }
        }
        for (double x : m_crossX) {
            for (double y : m_tabY) {
                placeSlot(SlotFeat{x - sw * 0.5, y - slTop * 0.5, x + sw * 0.5, y + slTop * 0.5});
            }
        }
        if (m_tabHoleHits.size() > 8) {
            m_errors << QStringLiteral("%1 more apron tabs cut a dog hole.").arg(m_tabHoleHits.size() - 8);
        }
        top.outer = plateWithNotches(spec.length, spec.width, notches);
        finishPart(top, spec.density);
        m_parts.push_back(std::move(top));
    }

    auto apronHoleDepths = [&]() {
        std::vector<double> d;
        if (!spec.apronHoles) {
            return d;
        }
        const double a = spec.apronHoleFromTop;
        const double b = spec.webDepth - spec.apronHoleFromBottom;
        if (a - holeR > 0.05 && b + holeR < spec.webDepth - 0.05 && b - a > holeR * 2 + sw) {
            d.push_back(a);
            d.push_back(b);
        } else {
            m_errors << QStringLiteral("Apron hole rows do not fit in the web depth, so those holes were omitted.");
        }
        return d;
    };
    const auto holeDepths = apronHoleDepths();

    const auto p2Tabs = spec.apronTopSlots
                            ? fitStations(m_apronTabX, A0, m_longApronLen, halfTab, QStringLiteral("Long apron tab"), m_errors)
                            : std::vector<double>{};
    const auto p2Laps = fitStations(m_crossX, A0, m_longApronLen, halfLap, QStringLiteral("Long apron receiver"), m_errors);
    std::vector<CircleFeat> p2Holes;
    std::vector<SlotFeat> p2Slots;
    for (double x : m_holeX) {
        const double local = x - A0;
        if (local - holeR > 0.15 && local + holeR < m_longApronLen - 0.15) {
            for (double d : holeDepths) {
                p2Holes.push_back({local, d, holeR});
            }
        }
    }
    for (double x : p2Laps) {
        p2Slots.push_back({x - halfLap, mid - slApron * 0.5, x + halfLap, mid + slApron * 0.5});
    }
    makeWeb(QStringLiteral("P02"), QStringLiteral("Long apron"), 2, m_longApronLen, false, p2Tabs, {}, false, p2Holes,
            p2Slots, {});

    const auto p3Tabs = spec.apronTopSlots
                            ? fitStations(m_apronTabY, m_apronInner, m_endApronLen, halfTab, QStringLiteral("End apron tab"), m_errors)
                            : std::vector<double>{};
    const auto p3Laps = fitStations(m_longY, m_apronInner, m_endApronLen, halfLap, QStringLiteral("End apron receiver"), m_errors);
    std::vector<CircleFeat> p3Holes;
    std::vector<SlotFeat> p3Slots;
    for (double y : m_holeY) {
        const double local = y - m_apronInner;
        if (local - holeR > 0.15 && local + holeR < m_endApronLen - 0.15) {
            for (double d : holeDepths) {
                p3Holes.push_back({local, d, holeR});
            }
        }
    }
    for (double y : p3Laps) {
        p3Slots.push_back({y - halfLap, mid - slApron * 0.5, y + halfLap, mid + slApron * 0.5});
    }
    makeWeb(QStringLiteral("P03"), QStringLiteral("End apron"), 2, m_endApronLen, false, p3Tabs, {}, false, p3Holes, p3Slots,
            {});

    const auto p4Tabs = fitStations(m_tabX, m_r0, m_longRibLen, halfTab, QStringLiteral("Long rib tab"), m_errors);
    const auto p4Laps = fitStations(m_crossX, m_r0, m_longRibLen, halfLap, QStringLiteral("Long rib half-lap"), m_errors);
    std::vector<double> p4Edges = {0.0};
    for (double x : p4Laps) {
        p4Edges.push_back(x);
    }
    p4Edges.push_back(m_longRibLen);
    std::sort(p4Edges.begin(), p4Edges.end());
    makeWeb(QStringLiteral("P04"), QStringLiteral("Long stiffener"), 2, m_longRibLen, true, p4Tabs, p4Laps, false, {}, {},
            p4Edges);

    const auto p5Tabs = fitStations(m_tabY, m_c0, m_crossRibLen, halfTab, QStringLiteral("Cross rib tab"), m_errors);
    const auto p5Laps = fitStations(m_longY, m_c0, m_crossRibLen, halfLap, QStringLiteral("Cross rib half-lap"), m_errors);
    std::vector<double> p5Edges = {0.0};
    for (double y : p5Laps) {
        p5Edges.push_back(y);
    }
    p5Edges.push_back(m_crossRibLen);
    std::sort(p5Edges.begin(), p5Edges.end());
    makeWeb(QStringLiteral("P05"), QStringLiteral("Cross stiffener"), static_cast<int>(m_crossX.size()), m_crossRibLen, true,
            p5Tabs, p5Laps, true, {}, {}, p5Edges);

    const double offsets[] = {-0.004, 0.0, 0.004, 0.008, 0.012};
    std::vector<SlotFeat> couponSlots;
    const double couponPitch = std::max(1.5, sw + 0.9);
    for (int i = 0; i < 5; ++i) {
        const double w = sw + offsets[i];
        const double x = 1.0 + i * couponPitch;
        couponSlots.push_back({x - w * 0.5, 1.5 - slTop * 0.5, x + w * 0.5, 1.5 + slTop * 0.5});
    }
    const double couponLen = std::max(9.0, 1.0 + 4.0 * couponPitch + 2.0);
    const double tongue0 = 4.0 - halfTab;
    const double tongue1 = 4.0 + halfTab;
    std::vector<QPointF> q1;
    addPt(q1, 0, 0);
    addPt(q1, tongue0, 0);
    addPt(q1, tongue0, -spec.tabHeight);
    addPt(q1, tongue1, -spec.tabHeight);
    addPt(q1, tongue1, 0);
    addPt(q1, couponLen, 0);
    addPt(q1, couponLen, 1);
    addPt(q1, couponLen + 0.2, 1);
    addPt(q1, couponLen + 0.2, 2);
    addPt(q1, couponLen, 2);
    addPt(q1, couponLen, 3);
    addPt(q1, 0, 3);
    auto addCoupon = [&](const QString& code, const QString& name, double thickness, bool tongue) {
        Part q;
        q.code = code;
        q.name = name;
        q.thickness = thickness;
        q.qty = 1;
        q.bodyLength = couponLen;
        if (tongue) {
            q.outer = q1;
        } else {
            q.outer = {QPointF(0, 0), QPointF(couponLen, 0), QPointF(couponLen, 3), QPointF(0, 3)};
        }
        q.slotCuts = couponSlots;
        q.holes.push_back({couponLen - 1.0, 1.5, holeR});
        finishPart(q, spec.density);
        m_parts.push_back(std::move(q));
    };
    addCoupon(QStringLiteral("Q01"), QStringLiteral("Thin-stock fit coupon"), spec.webThickness, true);
    if (spec.includeQ02) {
        addCoupon(QStringLiteral("Q02"), QStringLiteral("Top-stock fit coupon"), spec.topThickness, false);
    }

    // Fix P05 quantity if there are zero cross ribs: makeWeb already used size. If zero, still a part with qty 0
    // is confusing. Leave it; nest skips qty 0.
    if (const Part* p5 = find(QStringLiteral("P05"))) {
        if (p5->qty < 1) {
            m_warnings << QStringLiteral("No cross ribs were placed. Widen the top or reduce hole pitch.");
        }
    }

    // Tab footprints must sit inside a top slot.
    int covered = 0;
    const Part* top = find(QStringLiteral("P01"));
    if (top) {
        auto cover = [&](double x, double y, bool alongX) {
            const double hx = alongX ? halfTab : spec.webThickness * 0.5;
            const double hy = alongX ? spec.webThickness * 0.5 : halfTab;
            const double x0 = x - hx;
            const double y0 = y - hy;
            const double x1 = x + hx;
            const double y1 = y + hy;
            for (const SlotFeat& s : topCovers) {
                if (rectContains(s, x0, y0, x1, y1)) {
                    ++covered;
                    return;
                }
            }
            m_errors << QStringLiteral("Tab at X %1 Y %2 is not fully inside a top slot.")
                              .arg(QString::number(x, 'f', 3), QString::number(y, 'f', 3));
        };
        const double apronY0 = A0 + spec.webThickness * 0.5;
        const double apronY1 = spec.width - A0 - spec.webThickness * 0.5;
        const double apronX0 = A0 + spec.webThickness * 0.5;
        const double apronX1 = spec.length - A0 - spec.webThickness * 0.5;
        if (spec.apronTopSlots) {
            for (double y : std::vector<double>{apronY0, apronY1}) {
                for (double x : m_apronTabX) {
                    cover(x, y, true);
                }
            }
        }
        for (double y : m_longY) {
            for (double x : m_tabX) {
                cover(x, y, true);
            }
        }
        if (spec.apronTopSlots) {
            for (double x : std::vector<double>{apronX0, apronX1}) {
                for (double y : m_apronTabY) {
                    cover(x, y, false);
                }
            }
        }
        for (double x : m_crossX) {
            for (double y : m_tabY) {
                cover(x, y, false);
            }
        }
    }

    // Nest. Production thin parts, then the thin coupon.
    struct Item {
        int index;
        double w;
        double h;
    };
    std::vector<Item> items;
    auto pushQty = [&](const QString& code, int qty) {
        for (int i = 0; i < static_cast<int>(m_parts.size()); ++i) {
            if (m_parts[static_cast<size_t>(i)].code == code) {
                for (int n = 0; n < qty; ++n) {
                    items.push_back({i, m_parts[static_cast<size_t>(i)].bounds.width(),
                                     m_parts[static_cast<size_t>(i)].bounds.height()});
                }
                return;
            }
        }
    };
    if (const Part* p = find(QStringLiteral("P02"))) {
        pushQty(p->code, p->qty);
    }
    if (const Part* p = find(QStringLiteral("P04"))) {
        pushQty(p->code, p->qty);
    }
    if (const Part* p = find(QStringLiteral("P03"))) {
        pushQty(p->code, p->qty);
    }
    if (const Part* p = find(QStringLiteral("P05"))) {
        pushQty(p->code, p->qty);
    }
    if (spec.nestCoupon) {
        if (const Part* p = find(QStringLiteral("Q01"))) {
            pushQty(p->code, 1);
        }
    }

    double x = spec.nestMargin;
    double y = spec.nestMargin;
    double rowH = 0;
    const double limitX = spec.sheetLength - spec.nestMargin;
    const double limitY = spec.sheetWidth - spec.nestMargin;
    int seq = 1;
    int sheet = 0;
    m_nestOk = true;
    for (const Item& item : items) {
        if (item.w > limitX - spec.nestMargin + 1e-6 || item.h > limitY - spec.nestMargin + 1e-6) {
            m_nestOk = false;
            m_nestError = QStringLiteral("%1 is %2 x %3 in and does not fit on the %4 x %5 sheet.")
                              .arg(m_parts[static_cast<size_t>(item.index)].code)
                              .arg(QString::number(item.w, 'f', 2), QString::number(item.h, 'f', 2))
                              .arg(QString::number(spec.sheetLength, 'f', 1), QString::number(spec.sheetWidth, 'f', 1));
            break;
        }
        if (x + item.w > limitX + 1e-6) {
            x = spec.nestMargin;
            y += rowH + spec.nestGap;
            rowH = 0;
        }
        if (y + item.h > limitY + 1e-6) {
            ++sheet;
            x = spec.nestMargin;
            y = spec.nestMargin;
            rowH = 0;
        }
        const QRectF& b = m_parts[static_cast<size_t>(item.index)].bounds;
        Placement pl;
        pl.partIndex = item.index;
        pl.sheet = sheet;
        pl.dx = x - b.left();
        pl.dy = y - b.top();
        pl.label = QStringLiteral("%1-%2").arg(m_parts[static_cast<size_t>(item.index)].code).arg(seq, 2, 10, QChar('0'));
        m_nest.push_back(pl);
        ++seq;
        x += item.w + spec.nestGap;
        rowH = std::max(rowH, item.h);
    }
    if (m_nestOk && !m_nest.empty()) {
        m_nestSheets = m_nest.back().sheet + 1;
        m_nestOccupied = QRectF();
        for (const Placement& pl : m_nest) {
            if (pl.sheet != 0) {
                continue;
            }
            const QRectF b = m_parts[static_cast<size_t>(pl.partIndex)].bounds.translated(pl.dx, pl.dy);
            m_nestOccupied = m_nestOccupied.isNull() ? b : m_nestOccupied.united(b);
        }
    } else if (m_nestOk) {
        m_nestError = QStringLiteral("Nothing to nest.");
        m_nestOk = false;
    }
    if (!m_nestOk) {
        m_warnings << m_nestError;
        m_nest.clear();
        m_nestSheets = 0;
    }

    // Legs weld to the inside of the apron. Stringers tie them near the floor.
    const double leg = spec.tubeSize;
    const double str = std::min(spec.stringerSize, leg);
    const double ix = spec.apronInset + spec.webThickness;
    const double iy = spec.apronInset + spec.webThickness;
    const int pairs = std::max(2, spec.frameSupports);
    const double x0 = ix;
    const double xLast = spec.length - ix - leg;
    const double yFront = iy;
    const double yBack = spec.width - iy - leg;
    // Long stringers stop at the end cross tubes. They do not run out past the legs.
    m_railLength = std::max(0.0, xLast - x0 - leg);
    m_frameOuterW = yBack + leg - yFront;
    m_crossLength = 0;
    if (spec.frame && xLast >= x0 + leg && yBack >= yFront) {
        std::vector<double> xs(static_cast<size_t>(pairs), x0);
        xs.front() = x0;
        xs.back() = xLast;
        if (pairs > 2) {
            // Intermediate legs sit in a corner where a cross rib meets the apron.
            // The short center bay is too tight for a leg, so that bay is not used.
            const double halfWeb = spec.webThickness * 0.5;
            std::vector<std::pair<double, double>> walls = {{x0, x0 + leg}, {xLast, xLast + leg}};
            for (double c : m_crossX) {
                walls.push_back({c - halfWeb, c + halfWeb});
            }
            std::sort(walls.begin(), walls.end());
            std::vector<std::pair<double, double>> merged;
            for (const auto& w : walls) {
                if (!merged.empty() && w.first <= merged.back().second + 1e-6) {
                    merged.back().second = std::max(merged.back().second, w.second);
                } else {
                    merged.push_back(w);
                }
            }
            auto ribFace = [&](double edge) {
                for (double c : m_crossX) {
                    if (std::abs(edge - (c - halfWeb)) < 1e-3 || std::abs(edge - (c + halfWeb)) < 1e-3) {
                        return true;
                    }
                }
                return false;
            };
            std::vector<double> corners;
            for (size_t k = 0; k + 1 < merged.size(); ++k) {
                const double a = merged[k].second;
                const double b = merged[k + 1].first;
                if (b - a < leg + 1.0) {
                    continue;
                }
                if (ribFace(a)) {
                    corners.push_back(a);
                }
                if (ribFace(b)) {
                    corners.push_back(b - leg);
                }
            }
            std::vector<bool> used(corners.size(), false);
            for (int i = 1; i < pairs - 1; ++i) {
                const double nominal = x0 + (xLast - x0) * static_cast<double>(i) / static_cast<double>(pairs - 1);
                const double nominalCenter = nominal + leg * 0.5;
                int best = -1;
                double bestDist = 1e300;
                for (size_t k = 0; k < corners.size(); ++k) {
                    if (used[k]) {
                        continue;
                    }
                    bool hit = false;
                    for (int j = 0; j < i; ++j) {
                        const double ox = xs[static_cast<size_t>(j)];
                        if (corners[k] < ox + leg - 1e-4 && corners[k] + leg > ox + 1e-4) {
                            hit = true;
                            break;
                        }
                    }
                    if (hit) {
                        continue;
                    }
                    const double dist = std::abs(corners[k] + leg * 0.5 - nominalCenter);
                    if (dist < bestDist - 1e-6 || (std::abs(dist - bestDist) <= 1e-6 && (best < 0 || corners[k] < corners[static_cast<size_t>(best)]))) {
                        bestDist = dist;
                        best = static_cast<int>(k);
                    }
                }
                if (best >= 0) {
                    used[static_cast<size_t>(best)] = true;
                    xs[static_cast<size_t>(i)] = corners[static_cast<size_t>(best)];
                } else {
                    xs[static_cast<size_t>(i)] = nominal;
                    m_warnings << QStringLiteral("A leg pair has no open corner between a cross rib and the apron.");
                }
            }
        }
        for (double fx : xs) {
            m_frame.push_back({QRectF(QPointF(fx, yFront), QPointF(fx + leg, yFront + leg)), 2});
            m_frame.push_back({QRectF(QPointF(fx, yBack), QPointF(fx + leg, yBack + leg)), 2});
        }
        // Cross tubes are centered on every leg. In the single-stringer frame they run
        // through, and the center stringer is cut into the pieces that fit between them.
        const double tubeInset = (leg - str) * 0.5;
        const double xRun0 = xs.front() + (leg + str) * 0.5;
        const double xRun1 = xs.back() + tubeInset;
        double crossY0 = yFront + leg;
        double crossY1 = yBack;
        if (spec.doubleStringers) {
            m_frame.push_back({QRectF(QPointF(xRun0, yFront + leg), QPointF(xRun1, yFront + leg + str)), 3});
            m_frame.push_back({QRectF(QPointF(xRun0, yBack - str), QPointF(xRun1, yBack)), 3});
            crossY0 = yFront + leg + str;
            crossY1 = yBack - str;
            m_railLength = std::max(0.0, xRun1 - xRun0);
        }
        m_crossLength = std::max(0.0, crossY1 - crossY0);
        std::vector<double> crossLeft;
        if (m_crossLength > 0.5 && xRun1 - xRun0 > 0.5) {
            const int n = static_cast<int>(xs.size());
            crossLeft.reserve(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) {
                const double fx = xs[static_cast<size_t>(i)];
                const double sx = fx + tubeInset;
                crossLeft.push_back(sx);
                m_frame.push_back({QRectF(QPointF(sx, crossY0), QPointF(sx + str, crossY1)), 4});
            }
        }
        if (!spec.doubleStringers && crossLeft.size() >= 2) {
            const double mid = (yFront + leg + yBack) * 0.5;
            m_railLength = 0;
            for (size_t i = 0; i + 1 < crossLeft.size(); ++i) {
                const double a = crossLeft[i] + str;
                const double b = crossLeft[i + 1];
                if (b - a < 0.5) {
                    continue;
                }
                m_frame.push_back({QRectF(QPointF(a, mid - str * 0.5), QPointF(b, mid + str * 0.5)), 3});
                m_railLength = std::max(m_railLength, b - a);
            }
        }
        const double apronBottom = spec.finishedHeight - spec.topThickness - spec.webDepth;
        if (spec.stringerHeight + str > apronBottom + 1e-6) {
            m_warnings << QStringLiteral("Stringers reach the apron. They are usually kept near the floor.");
        }

        const double footSize = footPlateSize();
        Part foot;
        foot.code = QStringLiteral("F01");
        foot.name = QStringLiteral("Foot plate");
        foot.thickness = footPlateThickness();
        foot.qty = pairs * 2;
        foot.bodyLength = footSize;
        foot.outer = {QPointF(0, 0), QPointF(footSize, 0), QPointF(footSize, footSize), QPointF(0, footSize)};
        finishPart(foot, spec.density);
        m_parts.push_back(std::move(foot));

        std::vector<TubeBlank> blanks;
        auto addBlank = [&](double size, const QString& name, double length, int qty) {
            if (length < 0.5 || qty < 1) {
                return;
            }
            for (int n = 0; n < qty; ++n) {
                blanks.push_back({name, length, size});
            }
        };
        addBlank(leg, QStringLiteral("Leg"), legLengthExample(), pairs * 2);
        if (spec.doubleStringers) {
            addBlank(str, QStringLiteral("Long stringer"), m_railLength, 2);
        } else {
            for (const FrameRect& fr : m_frame) {
                if (fr.role == 3) {
                    addBlank(str, QStringLiteral("Center stringer"), fr.rect.width(), 1);
                }
            }
        }
        addBlank(str, QStringLiteral("Cross stringer"), m_crossLength, static_cast<int>(xs.size()));
        std::vector<double> sizes;
        for (const TubeBlank& blank : blanks) {
            bool seen = false;
            for (double size : sizes) {
                if (std::abs(size - blank.size) < 1e-6) {
                    seen = true;
                    break;
                }
            }
            if (!seen) {
                sizes.push_back(blank.size);
            }
        }
        for (double size : sizes) {
            std::vector<TubeBlank> group;
            for (const TubeBlank& blank : blanks) {
                if (std::abs(blank.size - size) < 1e-6) {
                    group.push_back(blank);
                }
            }
            std::vector<TubeStick> sticks20;
            std::vector<TubeStick> sticks24;
            const bool fit20 = packTubeSticks(group, kTubeStock20, sticks20);
            const bool fit24 = packTubeSticks(group, kTubeStock24, sticks24);
            if (!fit20 && !fit24) {
                m_warnings << QStringLiteral("A %1 in tube cut is longer than 24 ft.")
                                  .arg(QString::number(size, 'f', 3));
                continue;
            }
            const double cost20 = fit20 ? static_cast<double>(sticks20.size()) * kTubeStock20 : 1e300;
            const double cost24 = fit24 ? static_cast<double>(sticks24.size()) * kTubeStock24 : 1e300;
            const bool use20 = cost20 <= cost24;
            TubeNest nest;
            nest.size = size;
            nest.sticks = use20 ? sticks20 : sticks24;
            nest.stockLength = use20 ? kTubeStock20 : kTubeStock24;
            nest.stockFeet = nest.stockLength / 12.0;
            m_tubeNests.push_back(std::move(nest));
        }
    }

    m_assemblyWeight = 0;
    m_lighteningSaved = 0;
    for (const Part& p : m_parts) {
        if (p.code.startsWith(QLatin1Char('P')) || p.code.startsWith(QLatin1Char('F'))) {
            m_assemblyWeight += p.weight * p.qty;
        }
        if (p.code == QLatin1String("P04") || p.code == QLatin1String("P05")) {
            double cut = 0;
            for (const CapsuleFeat& c : p.caps) {
                cut += capsuleArea(c.length, c.height);
            }
            m_lighteningSaved += cut * p.qty * p.thickness * spec.density;
        }
    }

    noteCutGeometry(spec, m_parts, m_longY, m_crossX, m_apronInner, m_r0, m_longRibLen, m_c0, m_crossRibLen, m_errors,
                    m_warnings, m_clampBlocked);
    if (m_clampBlocked == 0) {
        m_checks << QStringLiteral("Every dog hole has at least 1.000 in from the hole edge to the nearest rib or apron.");
    } else {
        m_checks << QStringLiteral("%1 dog holes need a physical clamp check under the top.").arg(m_clampBlocked);
    }

    m_checks << QStringLiteral("%1 top holes, %2 top slots, %3 long ribs, %4 cross ribs.")
                    .arg(topHoleCount())
                    .arg(topSlotCount())
                    .arg(longRibCount())
                    .arg(crossRibCount());
    m_checks << QStringLiteral("%1 tab footprints sit inside receiver slots.").arg(covered);
    m_checks << QStringLiteral("Half-laps overlap by %1 in total so the ribs clear at mid-depth.")
                    .arg(QString::number(spec.halfLapExtra * 2.0, 'f', 3));
    if (m_nestOk && m_nestSheets <= 1) {
        m_checks << QStringLiteral("Nest places %1 profiles on one sheet. Occupied Y %2 to %3 in.")
                        .arg(m_nest.size())
                        .arg(QString::number(m_nestOccupied.top(), 'f', 3),
                             QString::number(m_nestOccupied.bottom(), 'f', 3));
    } else if (m_nestOk) {
        m_checks << QStringLiteral("Nest places %1 profiles on %2 sheets.")
                        .arg(m_nest.size())
                        .arg(m_nestSheets);
    }
    if (!m_tubeNests.empty()) {
        QStringList sticks;
        for (const TubeNest& nest : m_tubeNests) {
            sticks << QStringLiteral("%1 in on %2 ft x %3")
                          .arg(QString::number(nest.size, 'f', 3))
                          .arg(QString::number(nest.stockFeet, 'f', 0))
                          .arg(nest.sticks.size());
        }
        m_checks << QStringLiteral("Tube nest: %1. Saw kerf %2 in.")
                        .arg(sticks.join(QStringLiteral("; ")), QString::number(kTubeKerf, 'f', 3));
    }
    const double recess = spec.topThickness - spec.tabHeight;
    m_checks << QStringLiteral("Top tabs recess %1 in below the working face.").arg(QString::number(recess, 'f', 3));
    return true;
}
