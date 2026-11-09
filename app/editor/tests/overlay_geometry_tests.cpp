#include "OverlayLayer.h"
#include "NativeOverlay.h"
#include "WorkflowDocument.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QScreen>
#include <QTextStream>
#include <QWidget>
#include <cmath>

namespace {
int failures = 0;
int successes = 0;
QJsonArray failedChecks;
void check(bool ok, const char *message) {
    if (!ok) {
        QTextStream(stderr) << "FAIL: " << message << '\n'; ++failures;
        failedChecks.append(QString::fromUtf8(message));
    } else ++successes;
}
bool near(qreal a, qreal b) { return std::abs(a-b) < 0.0001; }
QRectF inventory(const WorkflowDocument &doc) {
    const auto r = doc.layout().value(QStringLiteral("inventory")).toObject().value(QStringLiteral("rect")).toObject();
    return {r.value(QStringLiteral("x")).toDouble(), r.value(QStringLiteral("y")).toDouble(),
            r.value(QStringLiteral("width")).toDouble(), r.value(QStringLiteral("height")).toDouble()};
}
QPointF point(const WorkflowDocument &doc, const QString &id) {
    const auto r = doc.layout().value(QStringLiteral("pointPositions")).toObject().value(id).toObject();
    return {r.value(QStringLiteral("x")).toDouble(), r.value(QStringLiteral("y")).toDouble()};
}
QWidget *find(const OverlayLayer &layer, const QString &name) {
    for (auto *widget : layer.interactiveWindows()) if (widget->objectName() == name) return widget;
    return nullptr;
}
void mouse(QWidget *widget, QEvent::Type type, const QPointF &global) {
    if (!widget) { check(false, "mouse target exists"); return; }
    const bool moving = type == QEvent::MouseMove;
    QMouseEvent event(type, global-QPointF(widget->pos()), global,
        moving ? Qt::NoButton : Qt::LeftButton,
        type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
}
void drag(QWidget *widget, const QPointF &delta) {
    if (!widget) { check(false, "drag target exists"); return; }
    const QPointF start = widget->geometry().center();
    mouse(widget, QEvent::MouseButtonPress, start);
    mouse(widget, QEvent::MouseMove, start+delta*.5);
    mouse(widget, QEvent::MouseMove, start+delta);
    mouse(widget, QEvent::MouseButtonRelease, start+delta);
}
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    WorkflowDocument document = WorkflowDocument::createDraft();
    document.setPointPosition(QStringLiteral("p1"), {.25, .25});
    auto positions = document.layout().value(QStringLiteral("pointPositions")).toObject();
    positions.insert(QStringLiteral("p2"), QJsonValue::Null);
    document.setLayout({{QStringLiteral("pointPositions"), positions}});
    QPointer<QWidget> drawingLifetime, controlLifetime;
    QJsonObject nativeReport;
    QJsonObject shortcutReport;
    {
        OverlayLayer layer(&document, 0, {});
        drawingLifetime = layer.drawingWindow();
        check(!layer.drawingWindow()->isVisible(), "constructor starts hidden");
        layer.show();
        app.processEvents();
        check(layer.drawingWindow()->geometry() == QGuiApplication::screens().first()->geometry(), "drawing covers full selected screen");
        check(layer.drawingWindow()->windowFlags().testFlag(Qt::WindowTransparentForInput), "Qt native input-transparent flag");
        check(layer.drawingWindow()->testAttribute(Qt::WA_TransparentForMouseEvents), "Qt mouse-transparent attribute");
        check(layer.interactiveWindows().size() == 13, "one calibrated point, four edges and eight handles only");
        check(find(layer, QStringLiteral("point:p2")) == nullptr, "null point has no widget");
        controlLifetime = find(layer, QStringLiteral("point:p1"));
        const QRectF invGlobal(layer.referenceRect().x()+inventory(document).x()*layer.referenceRect().width(),
            layer.referenceRect().y()+inventory(document).y()*layer.referenceRect().height(),
            inventory(document).width()*layer.referenceRect().width(), inventory(document).height()*layer.referenceRect().height());
        for (auto *widget : layer.interactiveWindows()) {
            check(widget->width() <= 29 || widget->height() <= 29, "no large interactive mouse trap");
            check(!widget->geometry().contains(invGlobal.center().toPoint()), "inventory interior is not an input window");
        }
        int started = 0, edited = 0, finished = 0, activated = 0, requested = 0;
        QJsonObject beforeStart, afterFinish;
        QObject::connect(&layer, &OverlayLayer::editStarted, [&] { ++started; beforeStart = document.data; });
        QObject::connect(&layer, &OverlayLayer::geometryEdited, [&] { ++edited; check(started > finished, "mutation inside history transaction"); layer.refresh(); });
        QObject::connect(&layer, &OverlayLayer::editFinished, [&] { ++finished; afterFinish = document.data; });
        QObject::connect(&layer, &OverlayLayer::pointActivated, [&](const QString &) { ++activated; });
        QObject::connect(&layer, &OverlayLayer::pointEditRequested, [&](const QString &) { ++requested; });
        auto *p1 = find(layer, QStringLiteral("point:p1"));
        int menus = 0;
        QObject::connect(&layer, &OverlayLayer::pointMenuRequested, [&](const QString &id, const QPoint &pos) {
            check(id == QStringLiteral("p1") && pos == p1->geometry().center(), "context menu carries point ID and global position");
            ++menus;
        });
        auto contextMenu = [&] {
            QContextMenuEvent event(QContextMenuEvent::Mouse, p1->rect().center(), p1->geometry().center());
            QApplication::sendEvent(p1, &event);
        };
        const auto original = document.data;
        contextMenu();
        check(menus == 1 && started == 0 && document.data == original, "right click requests menu without dragging or editing");
        mouse(p1, QEvent::MouseButtonPress, p1->geometry().center());
        mouse(p1, QEvent::MouseButtonRelease, p1->geometry().center());
        check(activated == 1 && started == 0 && finished == 0 && document.data == original, "click selects without history or mutation");
        const QRectF reference = layer.referenceRect();
        drag(p1, {reference.width()*.2, reference.height()*.15});
        check(near(point(document, QStringLiteral("p1")).x(), .45) && near(point(document, QStringLiteral("p1")).y(), .40), "point drag continues well outside tiny initial window");
        check(started == 1 && finished == 1 && edited >= 2, "continuous drag coalesces history");
        check(beforeStart == original && afterFinish == document.data, "history brackets all mutations");
        mouse(p1, QEvent::MouseButtonDblClick, p1->geometry().center());
        check(requested == 1, "double click requests point editor");
        drag(p1, {reference.width()*3, -reference.height()*3});
        check(point(document, QStringLiteral("p1")) == QPointF(1, 0), "point clamps to reference edges");

        const QPointF directions[] = {{-35,-35}, {0,-35}, {35,-35}, {35,0}, {35,35}, {0,35}, {-35,35}, {-35,0}};
        for (int handle = 0; handle < 8; ++handle) {
            document.setInventoryRect({.25, .25, .5, .4}); layer.refresh();
            const auto before = inventory(document);
            drag(find(layer, QStringLiteral("inventory-handle:%1").arg(handle)), directions[handle]);
            const auto after = inventory(document);
            check(after != before, "all eight resize handles mutate geometry");
            check(after.width() >= before.width() && after.height() >= before.height(), "outward handle grows expected dimensions");
        }
        document.setInventoryRect({.2, .2, .6, .6}); layer.refresh();
        drag(find(layer, QStringLiteral("inventory-handle:0")), {reference.width()*2, reference.height()*2});
        check(inventory(document).width()*reference.width() >= 11*16-.01, "inventory minimum width clamps before inversion");
        check(inventory(document).height()*reference.height() >= 3*16-.01, "inventory minimum height clamps before inversion");
        document.setInventoryRect({.01, .01, .02, .02}); layer.refresh();
        drag(find(layer, QStringLiteral("inventory-handle:0")), {100,100});
        check(inventory(document).width()*reference.width() >= 11*16-.01 &&
            inventory(document).height()*reference.height() >= 3*16-.01, "undersized loaded inventory near origin grows to minimum");
        document.setInventoryRect({.2, .2, .4, .3}); layer.refresh();
        drag(find(layer, QStringLiteral("inventory-edge:0")), {-reference.width()*3, reference.height()*3});
        check(near(inventory(document).x(), 0) && near(inventory(document).bottom(), 1), "inventory movement clamps at all screen bounds");
        check(near(inventory(document).width(), .4) && near(inventory(document).height(), .3), "move preserves inventory size");

        const QImage preview = layer.renderPreview(false);
        check(!preview.isNull() && preview == layer.renderPreview(false), "preview deterministic synthetic image, no screen grab");
        layer.setReadOnly(true);
        for (auto *widget : layer.interactiveWindows()) check(!widget->isVisible(), "readonly hides every interactive window");
        check(layer.drawingWindow()->isVisible(), "readonly preserves drawing layer");
        check(layer.renderPreview() == layer.renderPreview(false), "readonly preview has no handles");
        const auto readOnlyData = document.data;
        contextMenu();
        check(menus == 1, "readonly suppresses point context menu");
        drag(p1, {100,100});
        check(document.data == readOnlyData, "readonly blocks mutations even from sent widget events");
        layer.setReadOnly(false);
        layer.setEditingVisible(false);
        contextMenu();
        check(menus == 1, "hidden editing controls suppress point context menu");
        check(layer.drawingWindow()->isVisible(), "editing off leaves drawing visible");
        for (auto *widget : layer.interactiveWindows()) check(!widget->isVisible(), "editing off hides controls");
        layer.setEditingVisible(true);
        layer.setDrawingVisible(false);
        check(!layer.drawingWindow()->isVisible(), "drawing off hides drawing");
        for (auto *widget : layer.interactiveWindows()) check(!widget->isVisible(), "drawing off never leaves invisible hit regions");
        layer.setDrawingVisible(true);

        layer.setReferenceRect({.1,.2,.5,.6});
        check(near(layer.referenceRect().x(), reference.x()+reference.width()*.1), "crop preserves screen origin");
        check(near(layer.referenceRect().height(), reference.height()*.6), "normalized crop height");
        const auto validCrop = layer.referenceRect();
        layer.setReferenceRect({2,2,.1,.1});
        check(layer.referenceRect() == validCrop, "invalid crop ignored safely");
        QImage image(1600, 900, QImage::Format_ARGB32_Premultiplied); image.fill(QColor(QStringLiteral("#20323F"))); image.setDevicePixelRatio(2);
        layer.setBackground(image);
        check(layer.referencePixelSize() == QSize(800,540), "reference size uses image physical pixels, not image DPR twice");
        check(near(layer.referenceRect().width()/layer.referenceRect().height(), (1600.0*.5)/(900*.6)), "image fit then normalized crop, not screen crop");
        check(layer.drawingWindow()->geometry() == QGuiApplication::screens().first()->geometry(), "crop never shrinks full-screen drawing window");
        check(layer.diagnostics().value(QStringLiteral("backgroundBlocksDesktopInput")).toBool(), "screenshot backdrop blocks misleading desktop clicks");
        const auto referenceImage = layer.renderPreview(false);
        check(referenceImage != preview, "background supplied to safe preview");
        layer.setBackground({});
        check(!layer.diagnostics().value(QStringLiteral("backgroundBlocksDesktopInput")).toBool(), "live transparent overlay has no full-screen input blocker");
        layer.setReferenceRect({0,0,1,1});
        layer.setScreenIndex(9999);
        check(layer.referenceRect().size() == QGuiApplication::screens().last()->geometry().size(), "invalid screen index falls back safely");
        layer.setScreenIndex(0);
        for (int i = 0; i < QGuiApplication::screens().size(); ++i) {
            layer.setScreenIndex(i);
            const auto geometry = QGuiApplication::screens().at(i)->geometry();
            check(layer.drawingWindow()->geometry() == geometry && layer.referenceRect() == QRectF(geometry), "all selected screens preserve negative/global offsets");
            check(layer.referencePixelSize() == QSize(qRound(geometry.width()*QGuiApplication::screens().at(i)->devicePixelRatio()),
                qRound(geometry.height()*QGuiApplication::screens().at(i)->devicePixelRatio())), "screen DPR applied once to pixel reference size");
            document.setPointPosition(QStringLiteral("p1"), {.4,.4}); layer.refresh();
            drag(find(layer, QStringLiteral("point:p1")), {geometry.width()*.1, geometry.height()*.1});
            check(near(point(document, QStringLiteral("p1")).x(), .5) && near(point(document, QStringLiteral("p1")).y(), .5), "multi-screen drag converts global logical positions correctly");
        }
        layer.setScreenIndex(0);

        document = WorkflowDocument::createDraft();
        document.setPointPosition(QStringLiteral("p1"), {.12,.34});
        layer.refresh();
        check(find(layer, QStringLiteral("point:p2")) != nullptr, "in-place document assignment retains borrowed pointer");
        const auto moved = find(layer, QStringLiteral("point:p1"))->geometry().center();
        check(std::abs(moved.x()-(layer.referenceRect().x()+.12*layer.referenceRect().width())) <= 1.5, "refresh uses replacement document geometry");

        p1 = find(layer, QStringLiteral("point:p1"));
        const auto start = p1->geometry().center();
        mouse(p1, QEvent::MouseButtonPress, start);
        mouse(p1, QEvent::MouseMove, start+QPoint(100,100));
        const int finishedBeforeHide = finished;
        layer.hide();
        check(finished == finishedBeforeHide+1 && afterFinish == document.data, "hide ends in-flight history transaction after mutation");
        check(!layer.drawingWindow()->isVisible(), "hide hides drawing");
        for (auto *widget : layer.interactiveWindows()) check(!widget->isVisible(), "hide hides all controls");
        layer.show();
        const auto diag = layer.diagnostics();
        if (QGuiApplication::platformName() == QStringLiteral("cocoa")) {
            const auto native = diag.value(QStringLiteral("drawing")).toObject();
            check(native.value(QStringLiteral("nsWindowIgnoresMouseEvents")).toBool(), "actual NSWindow ignoresMouseEvents");
            check(native.value(QStringLiteral("nativeBorderless")).toBool(), "actual NSWindow borderless");
            for (auto *widget : layer.interactiveWindows())
                check(!nativeDiagnostics(widget).value(QStringLiteral("nsWindowIgnoresMouseEvents")).toBool(), "small Cocoa controls receive mouse events");
        }
        if (QGuiApplication::platformName() == QStringLiteral("windows")) {
            const auto native = diag.value(QStringLiteral("drawing")).toObject();
            check(native.value(QStringLiteral("nativeTransparent")).toBool() &&
                native.value(QStringLiteral("nativeLayered")).toBool(), "actual Windows layered transparent drawing");
            check(native.value(QStringLiteral("nativeNoActivate")).toBool() &&
                native.value(QStringLiteral("nativeBorderless")).toBool(), "actual Windows nonactivating borderless drawing");
        }
        check(!diag.value(QStringLiteral("recognition")).toBool() && !diag.value(QStringLiteral("inputInjection")).toBool(), "no recognition or input injection claims");
        nativeReport = diag;
    }
    check(drawingLifetime.isNull() && controlLifetime.isNull(), "destruction deletes owned top-level windows immediately");
    {
        NativeShortcuts shortcuts;
        if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
            check(!shortcuts.active() && !shortcuts.error().isEmpty(), "offscreen shortcut fallback never claims global support");
            int controls = 0, preview = 0;
            QObject::connect(&shortcuts, &NativeShortcuts::toggleControls, [&] { ++controls; });
            QObject::connect(&shortcuts, &NativeShortcuts::togglePreview, [&] { ++preview; });
            QWidget local;
            QKeyEvent p(QEvent::KeyPress, Qt::Key_P, Qt::ControlModifier | Qt::AltModifier);
            QKeyEvent d(QEvent::KeyPress, Qt::Key_D, Qt::ControlModifier | Qt::AltModifier);
            QApplication::sendEvent(&local, &p); QApplication::sendEvent(&local, &d);
            check(controls == 1 && preview == 1, "application-local fallback signals");
        }
        shortcutReport = {{QStringLiteral("globalHotkeys"), shortcuts.active()}, {QStringLiteral("error"), shortcuts.error()}};
    }
    const QJsonObject report{{QStringLiteral("passed"), failures == 0}, {QStringLiteral("failed"), failures},
        {QStringLiteral("passedChecks"), successes}, {QStringLiteral("failedChecks"), failedChecks},
        {QStringLiteral("platform"), QGuiApplication::platformName()},
        {QStringLiteral("liveOSEventInjection"), false}, {QStringLiteral("diagnostics"), nativeReport},
        {QStringLiteral("shortcuts"), shortcutReport}};
    QTextStream(stdout) << QJsonDocument(report).toJson(QJsonDocument::Compact) << '\n';
    QTextStream(stdout) << "overlay checks: " << (failures ? "FAILED" : "PASS") << " (" << failures << " failures)\n";
    return failures ? 1 : 0;
}
