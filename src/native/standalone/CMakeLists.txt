cmake_minimum_required(VERSION 3.25)
project(ThisIs395VJNative LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Test)
if(APPLE)
  add_executable(ThisIs395-VJ-ControlCenter MACOSX_BUNDLE
    ../NativeControlCenter.cpp
    ../NativeShowEditor.h
    ../NativeSpriteManager.h
    ../../resources/icons/icon.icns)
  set_source_files_properties(../../resources/icons/icon.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources")
  set_target_properties(ThisIs395-VJ-ControlCenter PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "ThisIs395 VJ Control Center"
    MACOSX_BUNDLE_ICON_FILE "icon.icns"
    MACOSX_BUNDLE_GUI_IDENTIFIER "net.thisis395.vj.controlcenter"
    MACOSX_BUNDLE_BUNDLE_VERSION "0.1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0")
elseif(WIN32)
  add_executable(ThisIs395-VJ-ControlCenter WIN32
    ../NativeControlCenter.cpp
    ../NativeShowEditor.h
    ../NativeSpriteManager.h
    ../../projectMSDL.rc)
else()
  add_executable(ThisIs395-VJ-ControlCenter
    ../NativeControlCenter.cpp
    ../NativeShowEditor.h
    ../NativeSpriteManager.h)
endif()
target_link_libraries(ThisIs395-VJ-ControlCenter PRIVATE Qt6::Widgets)
# Keep standalone Qt CI using the same exact logo as the unified build.
set(VJ_PORTRAIT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/../../../branding/portrait-original.png")
set_source_files_properties("${VJ_PORTRAIT_ICON}" PROPERTIES
    QT_RESOURCE_ALIAS "portrait.png")
qt_add_resources(ThisIs395-VJ-ControlCenter "thisis395_portrait"
    PREFIX "/thisis395" FILES "${VJ_PORTRAIT_ICON}")


# Widget-level regression coverage. The actual renderer IPC is separately
# exercised by tools/test_native_sprite_ipc.py in the unified Linux workflow.
enable_testing()
add_executable(ThisIs395-VJ-SpriteWidgetSmoke SpriteWidgetSmoke.cpp ../NativeSpriteManager.h)
target_link_libraries(ThisIs395-VJ-SpriteWidgetSmoke PRIVATE Qt6::Widgets Qt6::Test)
add_test(NAME NativeSpriteManagerButtons COMMAND ThisIs395-VJ-SpriteWidgetSmoke)
