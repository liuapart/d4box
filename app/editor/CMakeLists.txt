cmake_minimum_required(VERSION 3.24)
set(D4BOX_RELEASE_VERSION "0.2.5" CACHE STRING "Editor release version (without tag prefix)")
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" D4BOX_NUMERIC_VERSION "${D4BOX_RELEASE_VERSION}")
if(NOT D4BOX_NUMERIC_VERSION)
    message(FATAL_ERROR "D4BOX_RELEASE_VERSION must begin with X.Y.Z")
endif()
project(D4BoxEditor VERSION ${D4BOX_NUMERIC_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    add_compile_options(/utf-8 /W4)
endif()
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE MinSizeRel CACHE STRING "Build type" FORCE)
endif()

# Qt Base + SVG only: no QML, WebEngine, or Qt Network.
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Svg)
qt_standard_project_setup()
qt_add_executable(D4BoxEditor MACOSX_BUNDLE
    src/main.cpp src/EditorWindow.cpp src/EditorWindow.h
    src/EditorTheme.cpp src/EditorTheme.h
    src/EditorUpdates.cpp src/EditorUpdates.h
    src/WorkflowDocument.cpp src/WorkflowDocument.h
    src/EditorDialogs.cpp src/EditorDialogs.h
    src/WindowChrome.cpp src/WindowChrome.h
    src/PointNameDialog.cpp src/PointNameDialog.h
    src/WorkflowPanel.cpp src/WorkflowPanel.h
    src/OverlayLayer.cpp src/OverlayLayer.h
    src/NativeOverlay.cpp src/NativeOverlay.h)
if(APPLE)
    target_sources(D4BoxEditor PRIVATE resources/brand/app-icon.icns)
    set_source_files_properties(resources/brand/app-icon.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    set_target_properties(D4BoxEditor PROPERTIES MACOSX_BUNDLE_ICON_FILE "app-icon.icns")
elseif(WIN32)
    enable_language(RC)
    target_sources(D4BoxEditor PRIVATE resources/brand/editor-icon.rc)
    target_include_directories(D4BoxEditor PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources/brand")
endif()
target_link_libraries(D4BoxEditor PRIVATE Qt6::Widgets Qt6::Svg)
qt_add_resources(D4BoxEditor editor_brand PREFIX "/brand" BASE resources/brand
    FILES resources/brand/app-icon.svg)
qt_add_resources(D4BoxEditor editor_update_helpers PREFIX "/update" BASE resources/update
    FILES resources/update/replace-macos.sh resources/update/replace-windows.ps1
          resources/update/unpack-windows.ps1)
qt_add_resources(D4BoxEditor editor_theme PREFIX "/theme" BASE resources/theme
    FILES resources/theme/down.svg resources/theme/up.svg resources/theme/check.svg)
target_compile_definitions(D4BoxEditor PRIVATE
    D4BOX_EDITOR_VERSION="${D4BOX_RELEASE_VERSION}" QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
set_target_properties(D4BoxEditor PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.d4box.editor"
    MACOSX_BUNDLE_BUNDLE_NAME "D4BoxEditor"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported)
    set_property(TARGET D4BoxEditor PROPERTY INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
endif()
if(APPLE)
    enable_language(OBJCXX)
    set_source_files_properties(src/NativeOverlay.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_sources(D4BoxEditor PRIVATE src/NativeOverlay.mm)
    target_link_libraries(D4BoxEditor PRIVATE "-framework AppKit" "-framework Carbon")
    target_link_options(D4BoxEditor PRIVATE "$<$<CONFIG:MinSizeRel>:-Wl,-dead_strip>")
elseif(MSVC)
    target_link_libraries(D4BoxEditor PRIVATE user32)
    set_target_properties(D4BoxEditor PROPERTIES WIN32_EXECUTABLE TRUE)
    target_compile_options(D4BoxEditor PRIVATE /W4 /utf-8)
    target_link_options(D4BoxEditor PRIVATE /OPT:REF /OPT:ICF)
else()
    target_compile_options(D4BoxEditor PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(D4BoxEditorUpdateTests tests/update_tests.cpp src/EditorUpdates.cpp
        src/EditorUpdates.h src/EditorDialogs.cpp src/EditorDialogs.h src/WindowChrome.cpp
        src/EditorTheme.cpp)
    target_include_directories(D4BoxEditorUpdateTests PRIVATE src)
    target_link_libraries(D4BoxEditorUpdateTests PRIVATE Qt6::Widgets Qt6::Svg)
    qt_add_resources(D4BoxEditorUpdateTests update_test_images PREFIX "/theme" BASE resources/theme
        FILES resources/theme/down.svg resources/theme/up.svg resources/theme/check.svg)
    add_test(NAME editor-updates COMMAND D4BoxEditorUpdateTests)
    set_tests_properties(editor-updates PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 15)
    add_executable(D4BoxEditorDialogTests tests/dialog_tests.cpp
        src/EditorDialogs.cpp src/EditorDialogs.h src/WindowChrome.cpp src/EditorTheme.cpp)
    target_include_directories(D4BoxEditorDialogTests PRIVATE src)
    target_link_libraries(D4BoxEditorDialogTests PRIVATE Qt6::Widgets Qt6::Svg)
    qt_add_resources(D4BoxEditorDialogTests dialog_test_images PREFIX "/theme" BASE resources/theme
        FILES resources/theme/down.svg resources/theme/up.svg resources/theme/check.svg)
    add_test(NAME editor-dialogs COMMAND D4BoxEditorDialogTests)
    set_tests_properties(editor-dialogs PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 15)
    add_executable(D4BoxEditorThemeTests tests/theme_tests.cpp src/EditorTheme.cpp)
    target_include_directories(D4BoxEditorThemeTests PRIVATE src)
    target_link_libraries(D4BoxEditorThemeTests PRIVATE Qt6::Widgets Qt6::Svg)
    qt_add_resources(D4BoxEditorThemeTests theme_test_images PREFIX "/theme" BASE resources/theme
        FILES resources/theme/down.svg resources/theme/up.svg resources/theme/check.svg)
    add_test(NAME editor-theme-controls COMMAND D4BoxEditorThemeTests)
    set_tests_properties(editor-theme-controls PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 15)
    add_executable(D4BoxEditorCoreTests tests/core_tests.cpp src/WorkflowDocument.cpp)
    target_include_directories(D4BoxEditorCoreTests PRIVATE src)
    target_link_libraries(D4BoxEditorCoreTests PRIVATE Qt6::Core)
    add_executable(D4BoxEditorOverlayTests tests/overlay_geometry_tests.cpp
        src/OverlayLayer.cpp src/OverlayLayer.h src/NativeOverlay.cpp src/NativeOverlay.h
        src/WorkflowDocument.cpp src/WorkflowDocument.h)
    target_include_directories(D4BoxEditorOverlayTests PRIVATE src)
    target_link_libraries(D4BoxEditorOverlayTests PRIVATE Qt6::Widgets)
    if(APPLE)
        target_sources(D4BoxEditorOverlayTests PRIVATE src/NativeOverlay.mm)
        target_link_libraries(D4BoxEditorOverlayTests PRIVATE "-framework AppKit" "-framework Carbon")
        # QTBUG-147602: exercise native CGImage colorspace ownership even when
        # the widget tests use offscreen. No window server or game input needed.
        add_executable(D4BoxEditorNativeImageTests tests/native_image_tests.mm)
        target_link_libraries(D4BoxEditorNativeImageTests PRIVATE Qt6::Gui "-framework CoreGraphics")
        add_test(NAME editor-native-image COMMAND D4BoxEditorNativeImageTests)
        set_tests_properties(editor-native-image PROPERTIES TIMEOUT 15)
    elseif(WIN32)
        target_link_libraries(D4BoxEditorOverlayTests PRIVATE user32)
    endif()
    add_test(NAME editor-overlay-geometry COMMAND D4BoxEditorOverlayTests)
    add_test(NAME editor-core COMMAND D4BoxEditorCoreTests)
    add_test(NAME editor-ui-smoke COMMAND D4BoxEditor --smoke-test)
    add_test(NAME editor-ui-interactions COMMAND D4BoxEditor --ui-self-test)
    set_tests_properties(editor-core editor-ui-smoke editor-ui-interactions editor-overlay-geometry PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 15)
endif()
