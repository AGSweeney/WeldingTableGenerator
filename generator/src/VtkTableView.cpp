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

#include "VtkTableView.h"

#include "TableModel.h"

#include <QShowEvent>

#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkAppendPolyData.h>
#include <vtkAnnotatedCubeActor.h>
#include <vtkBoundingBox.h>
#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkCellPicker.h>
#include <vtkCommand.h>
#include <vtkCubeSource.h>
#include <vtkCylinderSource.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkLight.h>
#include <vtkLinearExtrusionFilter.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyDataNormals.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolygon.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkUnsignedCharArray.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

class vtkViewCubeOrientationWidget : public vtkOrientationMarkerWidget {
public:
    static vtkViewCubeOrientationWidget* New();
    vtkTypeMacro(vtkViewCubeOrientationWidget, vtkOrientationMarkerWidget);

    bool containsDisplayPoint(int x, int y) const {
        if (!this->Renderer) {
            return false;
        }
        vtkRenderWindow* rw = this->Renderer->GetRenderWindow();
        if (!rw) {
            return false;
        }
        int* sz = rw->GetSize();
        if (!sz || sz[0] <= 0 || sz[1] <= 0) {
            return false;
        }
        double vp[4] = {};
        this->Renderer->GetViewport(vp);
        const int xmin = static_cast<int>(std::floor(vp[0] * sz[0]));
        const int ymin = static_cast<int>(std::floor(vp[1] * sz[1]));
        const int xmax = static_cast<int>(std::ceil(vp[2] * sz[0]));
        const int ymax = static_cast<int>(std::ceil(vp[3] * sz[1]));
        return x >= xmin && x <= xmax && y >= ymin && y <= ymax;
    }

    int pickFaceByProjection(int x, int y) const {
        if (!this->Renderer || !containsDisplayPoint(x, y)) {
            return -1;
        }
        vtkCamera* cam = this->Renderer->GetActiveCamera();
        if (!cam) {
            return -1;
        }
        static const double centers[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        static const double normals[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        double camPos[3] = {};
        cam->GetPosition(camPos);
        double best = 1e300;
        int bestFace = -1;
        for (int i = 0; i < 6; ++i) {
            const double toCam[3] = {camPos[0] - centers[i][0], camPos[1] - centers[i][1], camPos[2] - centers[i][2]};
            if (vtkMath::Dot(normals[i], toCam) <= 0.0) {
                continue;
            }
            double disp[3] = {};
            vtkInteractorObserver::ComputeWorldToDisplay(this->Renderer, centers[i][0], centers[i][1], centers[i][2], disp);
            const double dx = x - disp[0];
            const double dy = y - disp[1];
            const double d2 = dx * dx + dy * dy;
            if (d2 < best) {
                best = d2;
                bestFace = i;
            }
        }
        return bestFace;
    }

    int pickFace(int x, int y) {
        if (!this->Renderer || !this->OrientationMarker || !containsDisplayPoint(x, y)) {
            return -1;
        }
        vtkNew<vtkCellPicker> picker;
        picker->SetTolerance(0.01);
        if (picker->Pick(x, y, 0.0, this->Renderer) && picker->GetCellId() >= 0) {
            const double* mp = picker->GetMapperPosition();
            if (mp) {
                const double ax = std::abs(mp[0]);
                const double ay = std::abs(mp[1]);
                const double az = std::abs(mp[2]);
                if (ax >= ay && ax >= az) {
                    return mp[0] >= 0.0 ? 0 : 1;
                }
                if (ay >= ax && ay >= az) {
                    return mp[1] >= 0.0 ? 2 : 3;
                }
                return mp[2] >= 0.0 ? 4 : 5;
            }
        }
        return pickFaceByProjection(x, y);
    }
};
vtkStandardNewMacro(vtkViewCubeOrientationWidget);

void cubeClickCallback(vtkObject*, unsigned long, void* client, void*) {
    if (auto* view = static_cast<VtkTableView*>(client)) {
        view->handleCubeClick();
    }
}

void cubeHoverCallback(vtkObject*, unsigned long, void* client, void*) {
    if (auto* view = static_cast<VtkTableView*>(client)) {
        view->handleCubeHover();
    }
}

void clipCallback(vtkObject*, unsigned long, void* client, void*) {
    if (auto* view = static_cast<VtkTableView*>(client)) {
        view->onCameraInteraction();
    }
}

namespace {

constexpr double kGridMinor = 6.0;
constexpr double kGridMajor = 12.0;
constexpr double kFloorMinHalf = 96.0;
constexpr double kSpaceDeadzone = 45.0;
constexpr double kSpaceNorm = 350.0;

double snappedHalf(double desired) {
    const double steps = std::ceil(std::max(0.0, desired) / kGridMajor);
    return std::max(kGridMajor, steps * kGridMajor);
}

int divisionsFor(double half, double step) {
    if (half <= 0.0 || step <= 0.0) {
        return 1;
    }
    return std::max(1, static_cast<int>(std::lround((2.0 * half) / step)));
}

void buildGrid(vtkPolyData* out, double cx, double cz, double half, int divisions, double y) {
    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> lines;
    const double step = (2.0 * half) / divisions;
    for (int i = 0; i <= divisions; ++i) {
        const double s = -half + step * i;
        vtkIdType a[2] = {points->InsertNextPoint(cx - half, y, cz + s), points->InsertNextPoint(cx + half, y, cz + s)};
        lines->InsertNextCell(2, a);
        vtkIdType b[2] = {points->InsertNextPoint(cx + s, y, cz - half), points->InsertNextPoint(cx + s, y, cz + half)};
        lines->InsertNextCell(2, b);
    }
    out->SetPoints(points);
    out->SetLines(lines);
}

void buildPlane(vtkPolyData* out, double cx, double cz, double half, double y) {
    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> polys;
    vtkIdType q[4] = {points->InsertNextPoint(cx - half, y, cz - half), points->InsertNextPoint(cx + half, y, cz - half),
                      points->InsertNextPoint(cx + half, y, cz + half), points->InsertNextPoint(cx - half, y, cz + half)};
    polys->InsertNextCell(4, q);
    out->SetPoints(points);
    out->SetPolys(polys);
}

void buildShade(vtkPolyData* out, double cx, double cz, double radius, double y, const unsigned char centerRgba[4],
                const unsigned char edgeRgba[4]) {
    constexpr int n = 96;
    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> polys;
    vtkNew<vtkUnsignedCharArray> colors;
    colors->SetNumberOfComponents(4);
    points->InsertNextPoint(cx, y, cz);
    colors->InsertNextTypedTuple(centerRgba);
    for (int i = 0; i < n; ++i) {
        const double a = 2.0 * vtkMath::Pi() * i / n;
        points->InsertNextPoint(cx + radius * std::cos(a), y, cz + radius * std::sin(a));
        colors->InsertNextTypedTuple(edgeRgba);
    }
    for (int i = 0; i < n; ++i) {
        vtkIdType tri[3] = {0, 1 + i, 1 + ((i + 1) % n)};
        polys->InsertNextCell(3, tri);
    }
    out->SetPoints(points);
    out->SetPolys(polys);
    out->GetPointData()->SetScalars(colors);
}

void styleLineActor(vtkActor* actor, double r, double g, double b, double opacity, double width) {
    actor->GetProperty()->SetRepresentationToWireframe();
    actor->GetProperty()->SetColor(r, g, b);
    actor->GetProperty()->SetOpacity(opacity);
    actor->GetProperty()->SetLineWidth(width);
    actor->GetProperty()->LightingOff();
    actor->PickableOff();
}

void styleFlatActor(vtkActor* actor, double r, double g, double b, double opacity) {
    actor->GetProperty()->SetColor(r, g, b);
    actor->GetProperty()->SetOpacity(opacity);
    actor->GetProperty()->LightingOff();
    actor->GetProperty()->SetAmbient(1.0);
    actor->GetProperty()->SetDiffuse(0.0);
    actor->PickableOff();
}

struct Mesh {
    vtkSmartPointer<vtkAppendPolyData> append = vtkSmartPointer<vtkAppendPolyData>::New();
    std::vector<vtkSmartPointer<vtkPolyData>> keep;

    void add(vtkPolyData* pd) {
        if (!pd || pd->GetNumberOfPoints() < 1) {
            return;
        }
        auto copy = vtkSmartPointer<vtkPolyData>::New();
        copy->DeepCopy(pd);
        keep.push_back(copy);
        append->AddInputData(copy);
    }

    void addBox(double x0, double y0, double z0, double x1, double y1, double z1) {
        if (x1 < x0) {
            std::swap(x0, x1);
        }
        if (y1 < y0) {
            std::swap(y0, y1);
        }
        if (z1 < z0) {
            std::swap(z0, z1);
        }
        if (x1 - x0 < 1e-4 || y1 - y0 < 1e-4 || z1 - z0 < 1e-4) {
            return;
        }
        vtkNew<vtkCubeSource> cube;
        cube->SetCenter(0.5 * (x0 + x1), 0.5 * (z0 + z1), 0.5 * (y0 + y1));
        cube->SetXLength(x1 - x0);
        cube->SetYLength(z1 - z0);
        cube->SetZLength(y1 - y0);
        cube->Update();
        add(cube->GetOutput());
    }

    void addHole(double vx, double vy, double vz, double radius, double height, int axis) {
        if (radius < 1e-4 || height < 1e-4) {
            return;
        }
        vtkNew<vtkCylinderSource> cyl;
        cyl->SetRadius(radius);
        cyl->SetHeight(height);
        cyl->SetResolution(48);
        cyl->CappingOn();
        cyl->Update();
        vtkNew<vtkTransform> tf;
        tf->PostMultiply();
        if (axis == 0) {
            tf->RotateZ(90.0);
        } else if (axis == 2) {
            tf->RotateX(90.0);
        }
        tf->Translate(vx, vy, vz);
        vtkNew<vtkTransformPolyDataFilter> filter;
        filter->SetTransform(tf);
        filter->SetInputData(cyl->GetOutput());
        filter->Update();
        add(filter->GetOutput());
    }

    vtkSmartPointer<vtkPolyData> finish() {
        if (keep.empty()) {
            return nullptr;
        }
        append->Update();
        auto out = vtkSmartPointer<vtkPolyData>::New();
        out->DeepCopy(append->GetOutput());
        return out;
    }
};

vtkSmartPointer<vtkActor> solidActor(vtkPolyData* pd, double r, double g, double b, bool hole) {
    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(pd);
    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(r, g, b);
    if (hole) {
        actor->GetProperty()->LightingOff();
        actor->GetProperty()->SetAmbient(1.0);
    } else {
        actor->GetProperty()->SetAmbient(0.38);
        actor->GetProperty()->SetDiffuse(0.62);
        actor->GetProperty()->SetSpecular(0.28);
        actor->GetProperty()->SetSpecularPower(40);
        actor->GetProperty()->SetInterpolationToPhong();
    }
    return actor;
}

vtkSmartPointer<vtkPolyData> withNormals(vtkPolyData* pd) {
    if (!pd) {
        return nullptr;
    }
    vtkNew<vtkPolyDataNormals> normals;
    normals->SetInputData(pd);
    normals->SetFeatureAngle(50);
    normals->ConsistencyOn();
    normals->SplittingOn();
    normals->Update();
    auto out = vtkSmartPointer<vtkPolyData>::New();
    out->DeepCopy(normals->GetOutput());
    return out;
}

void expandBounds(double b[6], double fraction) {
    for (int ax = 0; ax < 3; ++ax) {
        const double c = 0.5 * (b[2 * ax] + b[2 * ax + 1]);
        const double half = 0.5 * (b[2 * ax + 1] - b[2 * ax]) * (1.0 + fraction);
        b[2 * ax] = c - half;
        b[2 * ax + 1] = c + half;
    }
}

void applyIsometric(vtkRenderer* ren, const double b[6]) {
    vtkCamera* cam = ren->GetActiveCamera();
    const double cx = 0.5 * (b[0] + b[1]);
    const double cy = 0.5 * (b[2] + b[3]);
    const double cz = 0.5 * (b[4] + b[5]);
    const double diagonal = std::sqrt(std::pow(b[1] - b[0], 2) + std::pow(b[3] - b[2], 2) + std::pow(b[5] - b[4], 2));
    const double radius = 0.5 * std::max(diagonal, 1e-6);
    const double sinHalf = std::sin(0.5 * vtkMath::RadiansFromDegrees(cam->GetViewAngle()));
    const double dist = ((sinHalf > 1e-9) ? radius / sinHalf : radius * 2.5) * 1.08;
    double dir[3] = {0.75, 0.42, 0.51};
    vtkMath::Normalize(dir);
    cam->SetFocalPoint(cx, cy, cz);
    cam->SetPosition(cx + dir[0] * dist, cy + dir[1] * dist, cz + dir[2] * dist);
    cam->SetViewUp(0.0, 1.0, 0.0);
    cam->OrthogonalizeViewUp();
    cam->ParallelProjectionOff();
}

void applyOrtho(vtkRenderer* ren, const double b[6], VtkTableView::StandardView view) {
    vtkCamera* cam = ren->GetActiveCamera();
    const double cx = 0.5 * (b[0] + b[1]);
    const double cy = 0.5 * (b[2] + b[3]);
    const double cz = 0.5 * (b[4] + b[5]);
    const double hx = 0.5 * (b[1] - b[0]);
    const double hy = 0.5 * (b[3] - b[2]);
    const double hz = 0.5 * (b[5] - b[4]);
    const double dist = std::max(std::sqrt(4 * hx * hx + 4 * hy * hy + 4 * hz * hz) * 2.5, 1e-3);
    cam->SetFocalPoint(cx, cy, cz);
    cam->ParallelProjectionOn();
    switch (view) {
    case VtkTableView::Top:
        cam->SetPosition(cx, cy + dist, cz);
        cam->SetViewUp(0.0, 0.0, -1.0);
        cam->SetParallelScale(std::max(hx, hz) * 1.08);
        break;
    case VtkTableView::Bottom:
        cam->SetPosition(cx, cy - dist, cz);
        cam->SetViewUp(0.0, 0.0, 1.0);
        cam->SetParallelScale(std::max(hx, hz) * 1.08);
        break;
    case VtkTableView::Left:
        cam->SetPosition(cx - dist, cy, cz);
        cam->SetViewUp(0.0, 1.0, 0.0);
        cam->SetParallelScale(std::max(hy, hz) * 1.08);
        break;
    case VtkTableView::Right:
        cam->SetPosition(cx + dist, cy, cz);
        cam->SetViewUp(0.0, 1.0, 0.0);
        cam->SetParallelScale(std::max(hy, hz) * 1.08);
        break;
    case VtkTableView::Front:
        cam->SetPosition(cx, cy, cz + dist);
        cam->SetViewUp(0.0, 1.0, 0.0);
        cam->SetParallelScale(std::max(hx, hy) * 1.08);
        break;
    case VtkTableView::Back:
        cam->SetPosition(cx, cy, cz - dist);
        cam->SetViewUp(0.0, 1.0, 0.0);
        cam->SetParallelScale(std::max(hx, hy) * 1.08);
        break;
    default:
        break;
    }
    cam->OrthogonalizeViewUp();
}

double normAxis(int raw, double deadzone = kSpaceDeadzone) {
    const double v = static_cast<double>(raw);
    if (std::abs(v) < deadzone) {
        return 0.0;
    }
    return std::clamp(v / kSpaceNorm, -1.0, 1.0);
}

#ifdef Q_OS_WIN
int16_t readLeI16(const unsigned char* p) {
    return static_cast<int16_t>(static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8));
}
#endif

}  // namespace

struct VtkTableView::Impl {
    vtkRenderer* renderer = nullptr;
    vtkNew<vtkPolyData> floor;
    vtkNew<vtkPolyData> shade;
    vtkNew<vtkPolyData> minor;
    vtkNew<vtkPolyData> major;
    vtkNew<vtkActor> floorActor;
    vtkNew<vtkActor> shadeActor;
    vtkNew<vtkActor> minorActor;
    vtkNew<vtkActor> majorActor;
    bool groundReady = false;
    bool groundInScene = false;
    std::vector<vtkSmartPointer<vtkActor>> parts;
    vtkSmartPointer<vtkAnnotatedCubeActor> cube;
    vtkSmartPointer<vtkViewCubeOrientationWidget> cubeWidget;
    vtkNew<vtkCallbackCommand> clickCmd;
    vtkNew<vtkCallbackCommand> hoverCmd;
    vtkNew<vtkCallbackCommand> clipCmd;
    unsigned long clickId = 0;
    unsigned long hoverId = 0;
    bool cubeReady = false;
    bool clipReady = false;
    int hoverFace = -1;
    bool cameraReady = false;
    bool spaceRegistered = false;
    int tx = 0;
    int ty = 0;
    int tz = 0;
    int rx = 0;
    int ry = 0;
    int rz = 0;
    vtkSmartPointer<vtkInteractorStyleTrackballCamera> style;
};

VtkTableView::VtkTableView(QWidget* parent) : QVTKOpenGLNativeWidget(parent), d(new Impl) {
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral("border: none; outline: none;"));
    setEnableHiDPI(true);
    vtkNew<vtkRenderer> ren;
    ren->SetNearClippingPlaneTolerance(0.02);
    ren->UseFXAAOn();
    ren->GradientBackgroundOn();
    ren->SetBackground(0x07 / 255.0, 0x09 / 255.0, 0x0e / 255.0);
    ren->SetBackground2(0x12 / 255.0, 0x17 / 255.0, 0x1f / 255.0);
    ren->AutomaticLightCreationOff();
    auto key = vtkSmartPointer<vtkLight>::New();
    key->SetLightTypeToSceneLight();
    key->SetPositional(false);
    key->SetPosition(2.6, 3.4, 2.1);
    key->SetFocalPoint(0.0, 0.8, 0.0);
    key->SetIntensity(0.95);
    auto fill = vtkSmartPointer<vtkLight>::New();
    fill->SetLightTypeToSceneLight();
    fill->SetPositional(false);
    fill->SetPosition(-2.8, 1.7, -1.8);
    fill->SetColor(0.78, 0.84, 0.94);
    fill->SetIntensity(0.40);
    auto rim = vtkSmartPointer<vtkLight>::New();
    rim->SetLightTypeToSceneLight();
    rim->SetPositional(false);
    rim->SetPosition(0.0, -5.0, 6.0);
    rim->SetColor(0.8, 0.9, 1.0);
    rim->SetIntensity(0.50);
    auto head = vtkSmartPointer<vtkLight>::New();
    head->SetLightTypeToHeadlight();
    head->SetIntensity(0.42);
    ren->AddLight(key);
    ren->AddLight(fill);
    ren->AddLight(rim);
    ren->AddLight(head);
    d->renderer = ren.Get();
    renderWindow()->AddRenderer(d->renderer);
    renderWindow()->SetMultiSamples(8);
    d->style = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
}

VtkTableView::~VtkTableView() {
    releaseSpaceMouse();
    if (d->cubeWidget) {
        d->cubeWidget->SetEnabled(0);
    }
    delete d;
}

void VtkTableView::showEvent(QShowEvent* event) {
    QVTKOpenGLNativeWidget::showEvent(event);
    if (auto* iren = renderWindow()->GetInteractor()) {
        if (d->style && iren->GetInteractorStyle() != d->style) {
            iren->SetInteractorStyle(d->style);
        }
    }
    ensureCube();
    ensureSpaceMouse();
    if (!d->clipReady) {
        if (auto* iren = renderWindow()->GetInteractor()) {
            d->clipCmd->SetClientData(this);
            d->clipCmd->SetCallback(clipCallback);
            iren->AddObserver(vtkCommand::InteractionEvent, d->clipCmd);
            iren->AddObserver(vtkCommand::EndInteractionEvent, d->clipCmd);
            iren->AddObserver(vtkCommand::MouseWheelForwardEvent, d->clipCmd);
            iren->AddObserver(vtkCommand::MouseWheelBackwardEvent, d->clipCmd);
            d->clipReady = true;
        }
    }
    if (!d->cameraReady) {
        resetView();
    } else if (d->renderer) {
        onCameraInteraction();
        renderWindow()->Render();
    }
}

void VtkTableView::ensureCube() {
    if (d->cubeReady) {
        return;
    }
    auto* iren = renderWindow()->GetInteractor();
    if (!iren) {
        return;
    }
    d->cube = vtkSmartPointer<vtkAnnotatedCubeActor>::New();
    d->cube->SetXPlusFaceText("X+");
    d->cube->SetXMinusFaceText("X-");
    d->cube->SetYPlusFaceText("Y+");
    d->cube->SetYMinusFaceText("Y-");
    d->cube->SetZPlusFaceText("Z+");
    d->cube->SetZMinusFaceText("Z-");
    d->cube->SetFaceTextScale(0.35);
    d->cube->SetXFaceTextRotation(-90.0);
    d->cube->SetYFaceTextRotation(-90.0);
    d->cube->SetZFaceTextRotation(90.0);
    d->cube->SetFaceTextVisibility(1);
    d->cube->SetTextEdgesVisibility(0);
    d->cubeWidget = vtkSmartPointer<vtkViewCubeOrientationWidget>::New();
    d->cubeWidget->SetOrientationMarker(d->cube);
    d->cubeWidget->SetInteractor(iren);
    d->cubeWidget->SetViewport(0.88, 0.81, 0.99, 0.99);
    d->cubeWidget->SetEnabled(1);
    d->cubeWidget->InteractiveOff();
    d->clickCmd->SetClientData(this);
    d->clickCmd->SetCallback(cubeClickCallback);
    d->clickId = iren->AddObserver(vtkCommand::LeftButtonPressEvent, d->clickCmd, 20.0f);
    d->hoverCmd->SetClientData(this);
    d->hoverCmd->SetCallback(cubeHoverCallback);
    d->hoverId = iren->AddObserver(vtkCommand::MouseMoveEvent, d->hoverCmd, 20.0f);
    d->cubeReady = true;
    applyCubeTheme();
}

void VtkTableView::applyCubeTheme() {
    if (!d->cube) {
        return;
    }
    d->cube->GetCubeProperty()->SetColor(0.18, 0.21, 0.25);
    d->cube->GetXPlusFaceProperty()->SetColor(0.52, 0.34, 0.34);
    d->cube->GetXMinusFaceProperty()->SetColor(0.34, 0.23, 0.23);
    d->cube->GetYPlusFaceProperty()->SetColor(0.34, 0.51, 0.37);
    d->cube->GetYMinusFaceProperty()->SetColor(0.23, 0.34, 0.25);
    d->cube->GetZPlusFaceProperty()->SetColor(0.33, 0.41, 0.56);
    d->cube->GetZMinusFaceProperty()->SetColor(0.24, 0.30, 0.40);
    vtkProperty* props[6] = {d->cube->GetXPlusFaceProperty(),  d->cube->GetXMinusFaceProperty(), d->cube->GetYPlusFaceProperty(),
                             d->cube->GetYMinusFaceProperty(), d->cube->GetZPlusFaceProperty(),  d->cube->GetZMinusFaceProperty()};
    if (d->hoverFace >= 0 && d->hoverFace < 6 && props[d->hoverFace]) {
        props[d->hoverFace]->SetColor(1.0, 1.0, 1.0);
    }
}

void VtkTableView::handleCubeClick() {
    if (!d->cubeWidget) {
        return;
    }
    auto* iren = renderWindow()->GetInteractor();
    if (!iren) {
        return;
    }
    const int* e = iren->GetEventPosition();
    if (!e || !d->cubeWidget->containsDisplayPoint(e[0], e[1])) {
        return;
    }
    int face = d->cubeWidget->pickFace(e[0], e[1]);
    if (face < 0) {
        face = d->hoverFace;
    }
    StandardView view = Isometric;
    switch (face) {
    case 0:
        view = Right;
        break;
    case 1:
        view = Left;
        break;
    case 2:
        view = Top;
        break;
    case 3:
        view = Bottom;
        break;
    case 4:
        view = Front;
        break;
    case 5:
        view = Back;
        break;
    default:
        return;
    }
    frameCamera(view);
    d->clickCmd->SetAbortFlag(1);
}

void VtkTableView::handleCubeHover() {
    if (!d->cubeWidget) {
        return;
    }
    auto* iren = renderWindow()->GetInteractor();
    if (!iren) {
        return;
    }
    const int* e = iren->GetEventPosition();
    if (!e) {
        return;
    }
    int face = -1;
    if (d->cubeWidget->containsDisplayPoint(e[0], e[1])) {
        face = d->cubeWidget->pickFace(e[0], e[1]);
    }
    if (face == d->hoverFace) {
        return;
    }
    d->hoverFace = face;
    applyCubeTheme();
    renderWindow()->Render();
}

void VtkTableView::onCameraInteraction() {
    if (!d->renderer) {
        return;
    }
    vtkCamera* cam = d->renderer->GetActiveCamera();
    if (!cam) {
        return;
    }
    double pos[3] = {};
    double fp[3] = {};
    cam->GetPosition(pos);
    cam->GetFocalPoint(fp);
    const double dist = std::max(0.5, std::sqrt(std::pow(pos[0] - fp[0], 2) + std::pow(pos[1] - fp[1], 2) + std::pow(pos[2] - fp[2], 2)));
    const double nearClip = std::max(0.04, dist * 0.01);
    cam->SetClippingRange(nearClip, dist * 20.0);
}

bool VtkTableView::tableBounds(double b[6]) const {
    if (!d->renderer) {
        return false;
    }
    vtkBoundingBox box;
    vtkActorCollection* actors = d->renderer->GetActors();
    actors->InitTraversal();
    while (vtkActor* actor = actors->GetNextActor()) {
        if (!actor->GetPickable() || !actor->GetVisibility()) {
            continue;
        }
        double ab[6] = {};
        actor->GetBounds(ab);
        if (vtkMath::AreBoundsInitialized(ab)) {
            box.AddBounds(ab);
        }
    }
    if (!box.IsValid()) {
        return false;
    }
    box.GetBounds(b);
    expandBounds(b, 0.12);
    return true;
}

void VtkTableView::resetView() {
    frameCamera(Isometric);
}

void VtkTableView::frameCamera(StandardView view) {
    if (!d->renderer) {
        return;
    }
    double b[6] = {};
    if (!tableBounds(b)) {
        const double empty[6] = {-48, 48, 0, 36, -24, 24};
        applyIsometric(d->renderer, empty);
    } else if (view == Isometric) {
        applyIsometric(d->renderer, b);
    } else {
        applyOrtho(d->renderer, b, view);
    }
    d->cameraReady = true;
    onCameraInteraction();
    if (isVisible() && renderWindow()) {
        renderWindow()->Render();
    }
}

void VtkTableView::setModel(const TableModel* model) {
    if (!d->renderer) {
        return;
    }
    if (!d->groundReady) {
        auto bind = [](vtkPolyData* pd, vtkActor* actor, bool shade) {
            auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
            mapper->SetInputData(pd);
            if (shade) {
                mapper->ScalarVisibilityOn();
                mapper->SetScalarModeToUsePointData();
                mapper->SetColorModeToDirectScalars();
            }
            actor->SetMapper(mapper);
        };
        bind(d->floor, d->floorActor, false);
        bind(d->shade, d->shadeActor, true);
        bind(d->minor, d->minorActor, false);
        bind(d->major, d->majorActor, false);
        styleFlatActor(d->floorActor, 0.17, 0.19, 0.21, 0.98);
        styleFlatActor(d->shadeActor, 1, 1, 1, 1);
        styleLineActor(d->minorActor, 0.27, 0.30, 0.34, 0.28, 0.7);
        styleLineActor(d->majorActor, 0.35, 0.39, 0.45, 0.45, 1.1);
        d->groundReady = true;
    }
    if (!d->groundInScene) {
        d->renderer->AddActor(d->floorActor);
        d->renderer->AddActor(d->shadeActor);
        d->renderer->AddActor(d->minorActor);
        d->renderer->AddActor(d->majorActor);
        d->groundInScene = true;
    }

    const TableSpec spec = model ? model->spec() : TableSpec{};
    const bool framed = model && spec.frame && !model->frame().empty();
    double zWeb0 = 0;
    double zWeb1 = 0;
    double zTop1 = 0;
    double foot = 0;
    if (framed) {
        foot = std::max(0.02, spec.topThickness);
        zTop1 = spec.finishedHeight;
        zWeb1 = zTop1 - std::max(0.0, spec.topThickness);
        zWeb0 = zWeb1 - std::max(0.0, spec.webDepth);
    } else {
        zWeb1 = std::max(0.0, spec.webDepth);
        zTop1 = zWeb1 + std::max(0.0, spec.topThickness);
    }
    const double cx = spec.length * 0.5;
    const double cz = spec.width * 0.5;
    const double half = snappedHalf(std::max(kFloorMinHalf, std::max(spec.length, spec.width) * 1.35));
    buildPlane(d->floor, cx, cz, half, -0.08);
    const unsigned char shadeCenter[4] = {18, 20, 26, 0};
    const unsigned char shadeEdge[4] = {18, 20, 26, 48};
    buildShade(d->shade, cx, cz, std::max(48.0, half * 1.85), -0.06, shadeCenter, shadeEdge);
    buildGrid(d->minor, cx, cz, half, divisionsFor(half, kGridMinor), -0.04);
    buildGrid(d->major, cx, cz, half, divisionsFor(half, kGridMajor), -0.02);

    for (const auto& actor : d->parts) {
        d->renderer->RemoveActor(actor);
    }
    d->parts.clear();

    auto addMesh = [&](const vtkSmartPointer<vtkPolyData>& pd, double r, double g, double b, bool hole) {
        if (!pd) {
            return;
        }
        auto actor = solidActor(pd, r, g, b, hole);
        d->renderer->AddActor(actor);
        d->parts.push_back(actor);
    };

    if (!model || model->parts().empty()) {
        if (!d->cameraReady) {
            resetView();
        }
        return;
    }

    const double t = std::max(0.05, spec.webThickness);
    const double yFront = spec.apronInset;
    const double yBack = spec.width - spec.apronInset - t;
    const double xLeft = spec.apronInset;
    const double xRight = spec.length - spec.apronInset - t;

    Mesh plate;
    bool extruded = false;
    if (const Part* top = model->find(QStringLiteral("P01"))) {
        if (top->outer.size() >= 3) {
            vtkNew<vtkPoints> pts;
            vtkNew<vtkPolygon> poly;
            poly->GetPointIds()->SetNumberOfIds(static_cast<vtkIdType>(top->outer.size()));
            for (size_t i = 0; i < top->outer.size(); ++i) {
                pts->InsertNextPoint(top->outer[i].x(), zWeb1, top->outer[i].y());
                poly->GetPointIds()->SetId(static_cast<vtkIdType>(i), static_cast<vtkIdType>(i));
            }
            vtkNew<vtkCellArray> cells;
            cells->InsertNextCell(poly);
            vtkNew<vtkPolyData> outline;
            outline->SetPoints(pts);
            outline->SetPolys(cells);
            vtkNew<vtkLinearExtrusionFilter> extrude;
            extrude->SetInputData(outline);
            extrude->SetExtrusionTypeToVectorExtrusion();
            extrude->SetVector(0, std::max(0.05, spec.topThickness), 0);
            extrude->CappingOn();
            extrude->Update();
            plate.add(extrude->GetOutput());
            extruded = true;
        }
    }
    if (!extruded) {
        plate.addBox(0, 0, zWeb1, spec.length, spec.width, zTop1);
    }
    addMesh(withNormals(plate.finish()), 0.78, 0.80, 0.82, false);

    Mesh apron;
    apron.addBox(spec.apronInset, yFront, zWeb0, spec.length - spec.apronInset, yFront + t, zWeb1);
    apron.addBox(spec.apronInset, yBack, zWeb0, spec.length - spec.apronInset, yBack + t, zWeb1);
    const double yIn0 = spec.apronInset + t;
    const double yIn1 = spec.width - spec.apronInset - t;
    if (yIn1 > yIn0) {
        apron.addBox(xLeft, yIn0, zWeb0, xLeft + t, yIn1, zWeb1);
        apron.addBox(xRight, yIn0, zWeb0, xRight + t, yIn1, zWeb1);
    }
    addMesh(withNormals(apron.finish()), 0.55, 0.62, 0.68, false);

    Mesh ribs;
    for (double y : model->longY()) {
        ribs.addBox(model->ribOriginX(), y - t * 0.5, zWeb0, model->ribOriginX() + model->longRibLength(), y + t * 0.5, zWeb1);
    }
    for (double x : model->crossX()) {
        ribs.addBox(x - t * 0.5, model->ribOriginY(), zWeb0, x + t * 0.5, model->ribOriginY() + model->crossRibLength(), zWeb1);
    }
    addMesh(ribs.finish(), 0.70, 0.78, 0.72, false);

    if (framed) {
        Mesh legs;
        Mesh feet;
        Mesh stringers;
        for (const auto& fr : model->frame()) {
            const QRectF r = fr.rect.normalized();
            if (fr.role == 2) {
                legs.addBox(r.left(), r.top(), foot, r.right(), r.bottom(), zWeb1);
                feet.addBox(r.left() - 0.5, r.top() - 0.5, 0, r.right() + 0.5, r.bottom() + 0.5, foot);
            } else if (fr.role == 3 || fr.role == 4) {
                const double z0 = std::max(0.0, spec.stringerHeight);
                stringers.addBox(r.left(), r.top(), z0, r.right(), r.bottom(), z0 + std::max(0.05, spec.stringerSize));
            }
        }
        addMesh(stringers.finish(), 0.36, 0.43, 0.48, false);
        addMesh(legs.finish(), 0.24, 0.42, 0.24, false);
        addMesh(feet.finish(), 0.16, 0.30, 0.16, false);
    }

    Mesh rims;
    Mesh holes;
    const double plateH = std::max(0.05, zTop1 - zWeb1);
    const double face = 0.03;
    auto disc = [](Mesh& mesh, double vx, double vy, double vz, double radius, int axis) {
        mesh.addHole(vx, vy, vz, radius, face, axis);
    };
    if (const Part* top = model->find(QStringLiteral("P01"))) {
        for (const CircleFeat& h : top->holes) {
            disc(rims, h.x, zTop1 + 0.05, h.y, h.r, 1);
            disc(holes, h.x, zTop1 + 0.09, h.y, h.r * 0.58, 1);
        }
        for (const SlotFeat& s : top->slotCuts) {
            rims.addBox(s.x0, s.y0, zTop1 + 0.04, s.x1, s.y1, zTop1 + 0.07);
            const double inset = 0.04;
            holes.addBox(s.x0 + inset, s.y0 + inset, zTop1 + 0.07, s.x1 - inset, s.y1 - inset, zTop1 + 0.11);
        }
    }
    auto apronDiscs = [&](double vx, double vy, double vz, double radius, int axis, double nx, double ny, double nz) {
        const double rim = t * 0.5 + 0.05;
        const double core = t * 0.5 + 0.09;
        disc(rims, vx + nx * rim, vy + ny * rim, vz + nz * rim, radius, axis);
        disc(holes, vx + nx * core, vy + ny * core, vz + nz * core, radius * 0.58, axis);
        disc(rims, vx - nx * rim, vy - ny * rim, vz - nz * rim, radius, axis);
        disc(holes, vx - nx * core, vy - ny * core, vz - nz * core, radius * 0.58, axis);
    };
    if (const Part* longApron = model->find(QStringLiteral("P02"))) {
        for (const CircleFeat& h : longApron->holes) {
            const double x = spec.apronInset + h.x;
            const double zz = zWeb1 - h.y;
            apronDiscs(x, zz, yFront + t * 0.5, h.r, 2, 0, 0, 1);
            apronDiscs(x, zz, yBack + t * 0.5, h.r, 2, 0, 0, 1);
        }
    }
    if (const Part* endApron = model->find(QStringLiteral("P03"))) {
        for (const CircleFeat& h : endApron->holes) {
            const double y = spec.apronInset + t + h.x;
            const double zz = zWeb1 - h.y;
            apronDiscs(xLeft + t * 0.5, zz, y, h.r, 0, 1, 0, 0);
            apronDiscs(xRight + t * 0.5, zz, y, h.r, 0, 1, 0, 0);
        }
    }
    addMesh(rims.finish(), 0.16, 0.35, 0.54, true);
    addMesh(holes.finish(), 0.04, 0.06, 0.08, true);

    if (!d->cameraReady) {
        resetView();
    } else if (isVisible()) {
        onCameraInteraction();
        renderWindow()->Render();
    }
}

void VtkTableView::ensureSpaceMouse() {
#ifdef Q_OS_WIN
    if (d->spaceRegistered) {
        return;
    }
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) {
        return;
    }
    RAWINPUTDEVICE rid = {};
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x08;
    rid.dwFlags = 0;
    rid.hwndTarget = hwnd;
    if (RegisterRawInputDevices(&rid, 1, sizeof(rid)) == TRUE) {
        d->spaceRegistered = true;
    }
#else
    (void)0;
#endif
}

void VtkTableView::releaseSpaceMouse() {
#ifdef Q_OS_WIN
    if (!d || !d->spaceRegistered) {
        return;
    }
    RAWINPUTDEVICE rid = {};
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x08;
    rid.dwFlags = RIDEV_REMOVE;
    rid.hwndTarget = nullptr;
    if (RegisterRawInputDevices(&rid, 1, sizeof(rid)) == TRUE) {
        d->spaceRegistered = false;
    }
#else
    (void)0;
#endif
}

void VtkTableView::applySpaceMouse() {
    const double tx = normAxis(d->tx, 70.0);
    const double ty = normAxis(d->ty, 70.0);
    const double tz = normAxis(d->tz, 70.0);
    const double rx = normAxis(d->rx);
    const double ry = normAxis(d->ry);
    const double rz = normAxis(d->rz);
    if (tx == 0 && ty == 0 && tz == 0 && rx == 0 && ry == 0 && rz == 0) {
        return;
    }
    vtkCamera* cam = d->renderer ? d->renderer->GetActiveCamera() : nullptr;
    if (!cam) {
        return;
    }
    double pos[3] = {};
    double focal[3] = {};
    double up[3] = {};
    cam->GetPosition(pos);
    cam->GetFocalPoint(focal);
    cam->GetViewUp(up);
    double forward[3] = {focal[0] - pos[0], focal[1] - pos[1], focal[2] - pos[2]};
    if (vtkMath::Norm(forward) <= 1e-9) {
        return;
    }
    vtkMath::Normalize(forward);
    double viewUp[3] = {up[0], up[1], up[2]};
    if (vtkMath::Norm(viewUp) <= 1e-9) {
        viewUp[0] = 0;
        viewUp[1] = 1;
        viewUp[2] = 0;
    } else {
        vtkMath::Normalize(viewUp);
    }
    double right[3] = {};
    vtkMath::Cross(forward, viewUp, right);
    if (vtkMath::Norm(right) <= 1e-9) {
        return;
    }
    vtkMath::Normalize(right);
    vtkMath::Cross(right, forward, viewUp);
    vtkMath::Normalize(viewUp);

    double b[6] = {};
    double diag = 80.0;
    if (tableBounds(b)) {
        diag = std::sqrt(std::pow(b[1] - b[0], 2) + std::pow(b[3] - b[2], 2) + std::pow(b[5] - b[4], 2));
    }
    const double scale = std::clamp(diag * 0.0035, 0.25, 3.0);
    const double dx = -tx * scale;
    const double dy = tz * scale;
    const double dz = ty * scale;
    const bool parallel = cam->GetParallelProjection() != 0;
    const double delta[3] = {right[0] * dx + viewUp[0] * dy + (parallel ? 0.0 : forward[0] * dz),
                             right[1] * dx + viewUp[1] * dy + (parallel ? 0.0 : forward[1] * dz),
                             right[2] * dx + viewUp[2] * dy + (parallel ? 0.0 : forward[2] * dz)};
    cam->SetPosition(pos[0] + delta[0], pos[1] + delta[1], pos[2] + delta[2]);
    cam->SetFocalPoint(focal[0] + delta[0], focal[1] + delta[1], focal[2] + delta[2]);
    if (parallel && std::abs(dz) > 1e-9) {
        cam->SetParallelScale(std::clamp(cam->GetParallelScale() * (1.0 - dz * 0.85), 1e-3, 1e6));
    }
    if (std::abs(rz) > 1e-9) {
        cam->Azimuth(rz * 1.4);
    }
    if (std::abs(rx) > 1e-9) {
        cam->Elevation(rx * 1.4);
    }
    if (std::abs(ry) > 1e-9) {
        cam->Roll(ry * 1.4);
    }
    cam->OrthogonalizeViewUp();
    onCameraInteraction();
    renderWindow()->Render();
}

bool VtkTableView::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
#ifdef Q_OS_WIN
    Q_UNUSED(eventType);
    auto* msg = static_cast<MSG*>(message);
    if (!msg || msg->message != WM_INPUT) {
        return QVTKOpenGLNativeWidget::nativeEvent(eventType, message, result);
    }
    UINT rawSize = 0;
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(msg->lParam), RID_INPUT, nullptr, &rawSize, sizeof(RAWINPUTHEADER)) != 0
        || rawSize == 0) {
        return QVTKOpenGLNativeWidget::nativeEvent(eventType, message, result);
    }
    std::vector<unsigned char> rawBytes(rawSize);
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(msg->lParam), RID_INPUT, rawBytes.data(), &rawSize, sizeof(RAWINPUTHEADER))
        != rawSize) {
        return QVTKOpenGLNativeWidget::nativeEvent(eventType, message, result);
    }
    auto* raw = reinterpret_cast<RAWINPUT*>(rawBytes.data());
    if (!raw || raw->header.dwType != RIM_TYPEHID || raw->data.hid.dwSizeHid < 1 || raw->data.hid.dwCount < 1) {
        return QVTKOpenGLNativeWidget::nativeEvent(eventType, message, result);
    }
    const unsigned char* bytes = raw->data.hid.bRawData;
    const UINT packetSize = raw->data.hid.dwSizeHid;
    for (UINT i = 0; i < raw->data.hid.dwCount; ++i) {
        const unsigned char* packet = bytes + i * packetSize;
        const uint8_t reportId = packet[0];
        if (reportId == 1 && packetSize >= 13) {
            d->tx = readLeI16(packet + 1);
            d->ty = readLeI16(packet + 3);
            d->tz = readLeI16(packet + 5);
            d->rx = readLeI16(packet + 7);
            d->ry = readLeI16(packet + 9);
            d->rz = readLeI16(packet + 11);
            applySpaceMouse();
        } else if (reportId == 1 && packetSize >= 7) {
            d->tx = readLeI16(packet + 1);
            d->ty = readLeI16(packet + 3);
            d->tz = readLeI16(packet + 5);
            applySpaceMouse();
        } else if (reportId == 2 && packetSize >= 7) {
            d->rx = readLeI16(packet + 1);
            d->ry = readLeI16(packet + 3);
            d->rz = readLeI16(packet + 5);
            applySpaceMouse();
        }
    }
    if (result) {
        *result = 0;
    }
    return false;
#else
    return QVTKOpenGLNativeWidget::nativeEvent(eventType, message, result);
#endif
}
