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
#include "MainWindow.h"
#include "PackageWriter.h"
#include "TableModel.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QSurfaceFormat>
#include <QTextStream>
#include <QVTKOpenGLNativeWidget.h>

#include <cmath>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

void attachConsole() {
#ifdef Q_OS_WIN
    if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
        FILE* out = nullptr;
        FILE* err = nullptr;
        freopen_s(&out, "CONOUT$", "w", stdout);
        freopen_s(&err, "CONOUT$", "w", stderr);
    }
#else
    (void)0;
#endif
}

int selfCheck() {
    attachConsole();
    QString reportText;
    QTextStream out(&reportText);
    TableModel model;
    const bool built = model.rebuild(TableSpec::revA());
    bool ok = built && model.errors().isEmpty();
    auto expect = [&](bool pass, const QString& message) {
        if (!pass) {
            ok = false;
            out << "FAIL " << message << "\n";
        }
    };
    const Part* p2 = model.find(QStringLiteral("P02"));
    const Part* p3 = model.find(QStringLiteral("P03"));
    const Part* p4 = model.find(QStringLiteral("P04"));
    const Part* p5 = model.find(QStringLiteral("P05"));
    expect(model.topHoleCount() == 174, QStringLiteral("holes %1").arg(model.topHoleCount()));
    expect(model.topSlotCount() == 76, QStringLiteral("slots %1").arg(model.topSlotCount()));
    expect(model.longRibCount() == 2, QStringLiteral("long ribs %1").arg(model.longRibCount()));
    expect(model.crossRibCount() == 10, QStringLiteral("cross ribs %1").arg(model.crossRibCount()));
    expect(p2 && std::abs(p2->bounds.width() - 115.0) < 1e-6, QStringLiteral("P02 width"));
    expect(p2 && std::abs(p2->bounds.top() + 0.3) < 1e-6, QStringLiteral("P02 tab"));
    expect(p2 && std::abs(p2->bounds.height() - 6.3) < 1e-6, QStringLiteral("P02 height"));
    expect(p3 && std::abs(p3->bounds.width() - 22.528) < 1e-6, QStringLiteral("P03 width %1").arg(p3 ? p3->bounds.width() : -1));
    expect(p4 && std::abs(p4->bounds.left() + 0.2) < 1e-6, QStringLiteral("P04 left"));
    expect(p4 && std::abs(p4->bounds.width() - 114.908) < 1e-4, QStringLiteral("P04 width %1").arg(p4 ? p4->bounds.width() : -1));
    expect(p5 && std::abs(p5->bounds.width() - 22.908) < 1e-4, QStringLiteral("P05 width %1").arg(p5 ? p5->bounds.width() : -1));
    expect(model.nestOk(), model.nestError());
    expect(model.nest().size() == 17, QStringLiteral("nest count %1").arg(model.nest().size()));
    expect(model.nestOk() && std::abs(model.nestOccupied().top() - 0.5) < 1e-6, QStringLiteral("nest top"));
    expect(model.nestOk() && std::abs(model.nestOccupied().bottom() - 46.1) < 1e-3,
           QStringLiteral("nest bottom %1").arg(model.nestOccupied().bottom()));
    expect(model.nestSheetCount() == 1, QStringLiteral("nest sheets %1").arg(model.nestSheetCount()));
    TableSpec tight = TableSpec::revA();
    tight.sheetWidth = 36;
    TableModel multi;
    expect(multi.rebuild(tight) && multi.errors().isEmpty() && multi.nestOk() && multi.nestSheetCount() > 1,
           QStringLiteral("multi nest sheets %1 %2").arg(multi.nestSheetCount()).arg(multi.nestError()));
    expect(multi.nest().size() == model.nest().size(),
           QStringLiteral("multi nest count %1").arg(multi.nest().size()));
    TableSpec flush = TableSpec::revA();
    flush.apronInset = 0;
    TableModel flushModel;
    expect(flushModel.rebuild(flush) && flushModel.errors().isEmpty(), QStringLiteral("flush apron rebuild"));
    expect(flushModel.topSlotCount() == model.topSlotCount(),
           QStringLiteral("flush slots %1 vs %2").arg(flushModel.topSlotCount()).arg(model.topSlotCount()));
    const Part* flushTop = flushModel.find(QStringLiteral("P01"));
    expect(flushTop && flushTop->openEdgeSlots > 0,
           QStringLiteral("flush edge slots %1").arg(flushTop ? flushTop->openEdgeSlots : 0));
    const Part* foot = model.find(QStringLiteral("F01"));
    expect(foot && std::abs(foot->thickness - 0.375) < 1e-9, QStringLiteral("foot thickness"));
    expect(foot && foot->qty == 6, QStringLiteral("foot qty %1").arg(foot ? foot->qty : 0));
    expect(foot && std::abs(foot->bounds.width() - 4.0) < 1e-6, QStringLiteral("foot size"));
    expect(std::abs(model.legLengthExample() - 35.25) < 1e-6, QStringLiteral("leg length %1").arg(model.legLengthExample()));
    expect(model.tubeNests().size() == 2, QStringLiteral("tube groups %1").arg(model.tubeNests().size()));
    for (const TubeNest& nest : model.tubeNests()) {
        const bool stockOk = std::abs(nest.stockFeet - 20.0) < 1e-6 || std::abs(nest.stockFeet - 24.0) < 1e-6;
        expect(stockOk && !nest.sticks.empty(), QStringLiteral("tube stock %1 ft").arg(nest.stockFeet));
    }
    if (!model.errors().isEmpty()) {
        out << model.errors().join(QStringLiteral("\n")) << "\n";
    }
    if (!ok) {
        QStringList xs;
        for (double x : model.crossX()) {
            xs << QString::number(x, 'f', 3);
        }
        QStringList ys;
        for (double y : model.longY()) {
            ys << QString::number(y, 'f', 3);
        }
        out << "cross X: " << xs.join(QStringLiteral(", ")) << "\n";
        out << "long Y: " << ys.join(QStringLiteral(", ")) << "\n";
        out << model.warnings().join(QStringLiteral("\n")) << "\n";
    }
    out << (ok ? "SELF-CHECK OK\n" : "SELF-CHECK FAILED\n");
    out.flush();
    QFile report(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("self-check.txt")));
    if (report.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        report.write(reportText.toUtf8());
    }
    fprintf(stdout, "%s", reportText.toLocal8Bit().constData());
    fflush(stdout);
    return ok ? 0 : 1;
}

}  // namespace

int main(int argc, char* argv[]) {
    QStringList args;
    for (int i = 1; i < argc; ++i) {
        args << QString::fromLocal8Bit(argv[i]);
    }
    if (args.contains(QStringLiteral("--self-check"))) {
        QCoreApplication app(argc, argv);
        return selfCheck();
    }
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    if (args.size() >= 2 && args.at(0) == QLatin1String("--export")) {
        QApplication app(argc, argv);
        TableModel model;
        QString message;
        if (!model.rebuild(TableSpec::revA())) {
            message = model.errors().join(QStringLiteral("\n"));
        } else {
            const PackageResult result = writePackage(model, args.at(1));
            message = result.message;
            if (!result.ok) {
                QFile report(QStringLiteral("export-log.txt"));
                if (report.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    report.write(message.toUtf8());
                }
                return 1;
            }
        }
        QFile report(QStringLiteral("export-log.txt"));
        if (report.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            report.write(message.toUtf8());
        }
        return message.startsWith(QStringLiteral("Wrote")) ? 0 : 1;
    }

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("WeldingTable"));
    QCoreApplication::setApplicationName(QStringLiteral("Welding Table Generator"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app/welding-table.svg")));
    AppStyle::apply(app);
    MainWindow window;
    window.resize(1480, 920);
    window.show();
    return app.exec();
}
