#include "NativeOverlay.h"

#include <QMetaObject>
#include <QPointer>
#include <QWidget>
#import <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>

namespace {
NSWindow *windowFor(QWidget *widget) {
    NSView *view = (__bridge NSView *)(reinterpret_cast<void *>(widget->winId()));
    return [view window];
}
struct Registration {
    QPointer<NativeShortcuts> owner;
    EventHandlerRef handler = nullptr;
    EventHotKeyRef hotkeys[2] = {};
    ~Registration() {
        for (auto key : hotkeys) if (key) UnregisterEventHotKey(key);
        if (handler) RemoveEventHandler(handler);
    }
};
OSStatus hotkeyHandler(EventHandlerCallRef, EventRef event, void *data) {
    auto *registration = static_cast<Registration *>(data);
    EventHotKeyID id{};
    const OSStatus status = GetEventParameter(event, kEventParamDirectObject,
        typeEventHotKeyID, nullptr, sizeof(id), nullptr, &id);
    if (status != noErr || id.signature != 0x44344278 || !registration->owner)
        return eventNotHandledErr;
    if (id.id != 1 && id.id != 2) return eventNotHandledErr;
    QMetaObject::invokeMethod(registration->owner,
        id.id == 1 ? "toggleControls" : "togglePreview", Qt::QueuedConnection);
    return noErr;
}
}

void d4boxMacConfigure(QWidget *widget, bool passthrough) {
    NSWindow *window = windowFor(widget);
    if (!window) return;
    [window setIgnoresMouseEvents:passthrough ? YES : NO];
    [window setOpaque:NO];
    [window setBackgroundColor:[NSColor clearColor]];
    [window setHasShadow:NO];
    [window setStyleMask:([window styleMask] & ~(NSWindowStyleMaskTitled |
        NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable))];
    // Match Qt::Tool, NOT screen-saver/status level: parent toolbar stays usable.
    [window setLevel:NSFloatingWindowLevel];
    [window setHidesOnDeactivate:NO];
    // Qt tools default to MoveToActiveSpace. AppKit throws an NSException if
    // that mutually exclusive flag is retained alongside CanJoinAllSpaces.
    [window setCollectionBehavior:(([window collectionBehavior] &
        ~(NSWindowCollectionBehaviorMoveToActiveSpace | NSWindowCollectionBehaviorFullScreenPrimary)) |
        NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorFullScreenAuxiliary)];
    [window setMovableByWindowBackground:NO];
    if ([window isKindOfClass:[NSPanel class]])
        [(NSPanel *)window setBecomesKeyOnlyIfNeeded:YES];
}

QJsonObject d4boxMacDiagnostics(QWidget *widget) {
    NSWindow *window = windowFor(widget);
    if (!window) return {{QStringLiteral("nsWindowAvailable"), false}};
    const NSUInteger style = [window styleMask];
    return {{QStringLiteral("nsWindowAvailable"), true},
            {QStringLiteral("nsWindowIgnoresMouseEvents"), bool([window ignoresMouseEvents])},
            {QStringLiteral("nsWindowStyleMask"), double(style)},
            {QStringLiteral("nativeBorderless"), !(style & (NSWindowStyleMaskTitled | NSWindowStyleMaskResizable))},
            {QStringLiteral("nsWindowHasShadow"), bool([window hasShadow])},
            {QStringLiteral("nsWindowOpaque"), bool([window isOpaque])},
            {QStringLiteral("nsWindowLevel"), double([window level])},
            {QStringLiteral("nsWindowCollectionBehavior"), double([window collectionBehavior])},
            {QStringLiteral("nsWindowBackingScaleFactor"), double([window backingScaleFactor])},
            {QStringLiteral("nsWindowHidesOnDeactivate"), bool([window hidesOnDeactivate])}};
}

bool d4boxMacLeftButtonDown() { return ([NSEvent pressedMouseButtons] & 1u) != 0; }

void *d4boxMacRegisterShortcuts(NativeShortcuts *owner, QString *error) {
    auto registration = std::make_unique<Registration>();
    registration->owner = owner;
    const EventTypeSpec type{kEventClassKeyboard, kEventHotKeyPressed};
    OSStatus status = InstallEventHandler(GetApplicationEventTarget(), hotkeyHandler,
        1, &type, registration.get(), &registration->handler);
    if (status == noErr) {
        const UInt32 keys[2] = {kVK_ANSI_P, kVK_ANSI_D};
        for (UInt32 i = 0; i < 2; ++i) {
            status = RegisterEventHotKey(keys[i], controlKey | optionKey,
                EventHotKeyID{0x44344278, i + 1}, GetApplicationEventTarget(), 0,
                &registration->hotkeys[i]);
            if (status != noErr) break;
        }
    }
    if (status != noErr) {
        if (error) *error = QStringLiteral("Carbon global hotkeys unavailable (OSStatus %1); Ctrl+Alt+P/D may be in use.").arg(status);
        return nullptr; // RAII unregisters any partial registration.
    }
    return registration.release();
}

void d4boxMacUnregisterShortcuts(void *registration) {
    delete static_cast<Registration *>(registration);
}
