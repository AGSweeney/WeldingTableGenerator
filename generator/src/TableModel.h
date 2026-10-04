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

#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <vector>

struct TableSpec {
    double length = 116.0;
    double width = 24.0;
    double topThickness = 0.375;
    double webThickness = 0.236;
    double webDepth = 6.0;
    double holeDiameterMm = 16.0;
    bool holeDiameterInch = false;
    double holePitchX = 4.0;
    double holePitchY = 4.0;
    double holeMarginX = 2.0;
    double holeMarginY = 2.0;
    double apronInset = 0.5;
    double slotClearance = 0.010;
    double tabWidth = 1.0;
    double slotExtra = 0.020;
    double tabHeight = 0.300;
    double endTabLength = 0.200;
    double endTabHeight = 1.0;
    double ribEndGap = 0.010;
    double crossRibSpacing = 12.0;
    double longRibSpacing = 8.0;
    bool apronTopSlots = true;
    bool apronHoles = true;
    double apronHoleFromTop = 1.0;
    double apronHoleFromBottom = 1.0;
    double halfLapExtra = 0.005;
    bool lightening = true;
    double openingHeight = 2.0;
    double landWide = 3.0;
    double landNarrow = 1.75;
    double landThreshold = 10.0;
    double openingGap = 2.0;
    double openingMax = 6.0;
    double openingMin = 2.0;
    double openingSnap = 0.5;
    bool frame = true;
    double tubeSize = 3.0;
    double stringerSize = 2.0;
    double stringerHeight = 4.0;
    bool doubleStringers = false;
    double tubeWall = 0.1875;
    double frameInsetX = 2.0;
    double frameInsetY = 2.0;
    int frameSupports = 3;
    double finishedHeight = 36.0;
    double footExtension = 1.0;
    double sheetLength = 120.0;
    double sheetWidth = 60.0;
    double nestGap = 0.250;
    double nestMargin = 0.500;
    bool nestCoupon = true;
    bool includeQ02 = true;
    double density = 0.2836;
    QString revision = QStringLiteral("A");
    QString title = QStringLiteral("Welding table");
    QString shopNotes;
    bool writeIndividuals = true;
    bool writeNest = true;
    bool writeFrame = true;
    bool writePdf = true;
    bool writeReadme = true;
    bool writeJson = true;

    static TableSpec revA();
    QJsonObject toJson() const;
    static TableSpec fromJson(const QJsonObject& obj, const TableSpec& fallback = {});
};

struct CircleFeat {
    double x = 0;
    double y = 0;
    double r = 0;
};

struct SlotFeat {
    double x0 = 0;
    double y0 = 0;
    double x1 = 0;
    double y1 = 0;
};

struct CapsuleFeat {
    double x = 0;
    double y = 0;
    double length = 0;
    double height = 0;
};

struct Part {
    QString code;
    QString name;
    double thickness = 0;
    int qty = 1;
    std::vector<QPointF> outer;
    std::vector<CircleFeat> holes;
    std::vector<SlotFeat> slotCuts;
    int openEdgeSlots = 0;
    std::vector<CapsuleFeat> caps;
    QRectF bounds;
    double area = 0;
    double weight = 0;
    double bodyLength = 0;
};

struct Placement {
    int partIndex = -1;
    int sheet = 0;
    double dx = 0;
    double dy = 0;
    QString label;
};

struct FrameRect {
    QRectF rect;
    int role = 0;  // 2 leg, 3 long stringer, 4 cross stringer
};

inline constexpr double kTubeKerf = 0.125;
inline constexpr double kTubeStock20 = 240.0;
inline constexpr double kTubeStock24 = 288.0;

struct TubePiece {
    QString name;
    double length = 0;
};

struct TubeStick {
    double size = 0;
    double stockLength = 0;
    double stockFeet = 0;
    double used = 0;
    std::vector<TubePiece> pieces;
};

struct TubeNest {
    double size = 0;
    double stockLength = 0;
    double stockFeet = 0;
    std::vector<TubeStick> sticks;
};

class TableModel {
public:
    bool rebuild(const TableSpec& spec);

    const TableSpec& spec() const { return m_spec; }
    const QStringList& errors() const { return m_errors; }
    const QStringList& warnings() const { return m_warnings; }
    const QStringList& checks() const { return m_checks; }
    const std::vector<Part>& parts() const { return m_parts; }
    const Part* find(const QString& code) const;

    const std::vector<double>& holeX() const { return m_holeX; }
    const std::vector<double>& holeY() const { return m_holeY; }
    const std::vector<double>& tabX() const { return m_tabX; }
    const std::vector<double>& tabY() const { return m_tabY; }
    const std::vector<double>& apronTabX() const { return m_apronTabX; }
    const std::vector<double>& apronTabY() const { return m_apronTabY; }
    const std::vector<SlotFeat>& tabHoleHits() const { return m_tabHoleHits; }
    const std::vector<double>& crossX() const { return m_crossX; }
    const std::vector<double>& longY() const { return m_longY; }

    int topHoleCount() const;
    int topSlotCount() const;
    int longRibCount() const { return static_cast<int>(m_longY.size()); }
    int crossRibCount() const { return static_cast<int>(m_crossX.size()); }

    const std::vector<Placement>& nest() const { return m_nest; }
    bool nestOk() const { return m_nestOk; }
    int nestSheetCount() const { return m_nestSheets; }
    const QString& nestError() const { return m_nestError; }
    QRectF nestOccupied() const { return m_nestOccupied; }

    double assemblyWeight() const { return m_assemblyWeight; }
    double lighteningSaved() const { return m_lighteningSaved; }
    double slotWidth() const;
    double topSlotLength() const;
    double apronSlotLength() const;
    double holeDiameterIn() const;
    QString dogHoleLabel() const;
    int clampBlockedHoles() const { return m_clampBlocked; }

    double longApronLength() const { return m_longApronLen; }
    double endApronLength() const { return m_endApronLen; }
    double longRibLength() const { return m_longRibLen; }
    double crossRibLength() const { return m_crossRibLen; }
    double apronInner() const { return m_apronInner; }
    double ribOriginX() const { return m_r0; }
    double ribOriginY() const { return m_c0; }

    const std::vector<FrameRect>& frame() const { return m_frame; }
    const std::vector<TubeNest>& tubeNests() const { return m_tubeNests; }
    double railLength() const { return m_railLength; }
    double crossmemberLength() const { return m_crossLength; }
    double frameOuterWidth() const { return m_frameOuterW; }
    double legLengthExample() const;
    double footPlateThickness() const { return m_spec.topThickness; }
    double footPlateSize() const { return m_spec.tubeSize + 1.0; }

    QString summary() const;

private:
    TableSpec m_spec;
    QStringList m_errors;
    QStringList m_warnings;
    QStringList m_checks;
    std::vector<Part> m_parts;
    std::vector<double> m_holeX;
    std::vector<double> m_holeY;
    std::vector<double> m_tabX;
    std::vector<double> m_tabY;
    std::vector<double> m_apronTabX;
    std::vector<double> m_apronTabY;
    std::vector<SlotFeat> m_tabHoleHits;
    std::vector<double> m_crossX;
    std::vector<double> m_longY;
    std::vector<Placement> m_nest;
    std::vector<FrameRect> m_frame;
    std::vector<TubeNest> m_tubeNests;
    bool m_nestOk = false;
    int m_nestSheets = 0;
    QString m_nestError;
    QRectF m_nestOccupied;
    double m_assemblyWeight = 0;
    double m_lighteningSaved = 0;
    double m_longApronLen = 0;
    double m_endApronLen = 0;
    double m_longRibLen = 0;
    double m_crossRibLen = 0;
    double m_apronInner = 0;
    double m_r0 = 0;
    double m_c0 = 0;
    double m_railLength = 0;
    double m_crossLength = 0;
    double m_frameOuterW = 0;
    int m_clampBlocked = 0;
};
