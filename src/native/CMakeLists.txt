# Native Qt controls and the SDL renderer can share a single executable.
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

if(ENABLE_VJ_SINGLE_BINARY AND APPLE)
    add_executable(ThisIs395-VJ-ControlCenter MACOSX_BUNDLE
        NativeControlCenter.cpp NativeShowEditor.h NativeSpriteManager.h)
else()
    add_executable(ThisIs395-VJ-ControlCenter WIN32
        NativeControlCenter.cpp NativeShowEditor.h NativeSpriteManager.h)
endif()

target_link_libraries(ThisIs395-VJ-ControlCenter PRIVATE Qt6::Widgets)
set_target_properties(ThisIs395-VJ-ControlCenter PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

if(ENABLE_VJ_SINGLE_BINARY)
    # Qt launches this very executable again as --vj-renderer. The second
    # instance hosts the separate native SDL/OpenGL visualizer window.
    target_sources(ThisIs395-VJ-ControlCenter PRIVATE
        ../main.cpp ../AudioCapture.cpp ../AudioCapture.h
        ../FPSLimiter.cpp ../FPSLimiter.h
        ../ProjectMSDLApplication.cpp ../ProjectMSDLApplication.h
        ../ProjectMWrapper.cpp ../ProjectMWrapper.h
        ../RenderLoop.cpp ../RenderLoop.h
        ../SDLRenderingWindow.cpp ../SDLRenderingWindow.h)

    target_compile_definitions(ThisIs395-VJ-ControlCenter PRIVATE
        THISIS395_VJ_EMBEDDED_RENDERER
        PROJECTMSDL_VERSION="${PROJECT_VERSION}"
        PROJECTMSDL_CONFIG_LOCATION="${DEFAULT_CONFIG_PATH}")

    if(ENABLE_GLES)
        target_compile_definitions(ThisIs395-VJ-ControlCenter PRIVATE USE_GLES)
    endif()
    if(TARGET GLEW::glew OR TARGET GLEW::glew_s)
        target_compile_definitions(ThisIs395-VJ-ControlCenter PRIVATE USE_GLEW)
    endif()

    if(WIN32)
        target_sources(ThisIs395-VJ-ControlCenter PRIVATE
            ../AudioCaptureImpl_WASAPI.cpp ../AudioCaptureImpl_WASAPI.h
            ../projectMSDL.rc)
        target_compile_definitions(ThisIs395-VJ-ControlCenter PRIVATE
            AUDIO_IMPL_HEADER="AudioCaptureImpl_WASAPI.h")
        set_target_properties(ThisIs395-VJ-ControlCenter PROPERTIES
            OUTPUT_NAME "ThisIs395-VJ" VS_DPI_AWARE "PerMonitor")
    elseif(APPLE)
        target_sources(ThisIs395-VJ-ControlCenter PRIVATE
            ../AudioCaptureImpl_macOS.mm ../AudioCaptureImpl_macOS.h
            ../AudioCaptureImpl_CoreAudioTap.mm ../AudioCaptureImpl_CoreAudioTap.h
            ../AudioCaptureImpl_SDL.cpp ../AudioCaptureImpl_SDL.h)
        set_source_files_properties(../AudioCaptureImpl_SDL.cpp PROPERTIES
            COMPILE_DEFINITIONS "AudioCaptureImpl=SdlAudioCapture")
        target_compile_definitions(ThisIs395-VJ-ControlCenter PRIVATE
            AUDIO_IMPL_HEADER="AudioCaptureImpl_macOS.h")
        target_link_libraries(ThisIs395-VJ-ControlCenter PRIVATE
            "-framework CoreAudio" "-framework AudioToolbox" "-framework Foundation")
        set_target_properties(ThisIs395-VJ-ControlCenter PROPERTIES
            OUTPUT_NAME "projectM"
            MACOSX_BUNDLE_BUNDLE_NAME "ThisIs395 VJ"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "net.thisis395.vj"
            MACOSX_BUNDLE_ICON_FILE "projectMSDL.icns"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/src/resources/Info.plist.in")
    else()
        target_sources(ThisIs395-VJ-ControlCenter PRIVATE
            ../AudioCaptureImpl_SDL.cpp ../AudioCaptureImpl_SDL.h)
        target_compile_definitions(ThisIs395-VJ-ControlCenter PRIVATE
            AUDIO_IMPL_HEADER="AudioCaptureImpl_SDL.h")
        set_target_properties(ThisIs395-VJ-ControlCenter PROPERTIES
            OUTPUT_NAME "ThisIs395-VJ")
    endif()

    target_include_directories(ThisIs395-VJ-ControlCenter PRIVATE "${CMAKE_SOURCE_DIR}/src")
    target_link_libraries(ThisIs395-VJ-ControlCenter PRIVATE
        ProjectMSDL-GUI ProjectMSDL-Notifications
        libprojectM::playlist Poco::Util Poco::JSON
        SDL2::SDL2$<$<STREQUAL:${SDL2_LINKAGE},static>:-static>
        OpenGL::$<IF:$<BOOL:${ENABLE_GLES}>,GLES3,GL>)

    if(APPLE)
        install(TARGETS ThisIs395-VJ-ControlCenter
            BUNDLE DESTINATION . COMPONENT projectMSDL)
    else()
        install(TARGETS ThisIs395-VJ-ControlCenter
            RUNTIME DESTINATION "${PROJECTMSDL_BIN_DIR}" COMPONENT projectMSDL)
    endif()
else()
    set_target_properties(ThisIs395-VJ-ControlCenter PROPERTIES
        OUTPUT_NAME "ThisIs395-VJ-ControlCenter")
    install(TARGETS ThisIs395-VJ-ControlCenter
        RUNTIME DESTINATION "${PROJECTMSDL_BIN_DIR}" COMPONENT projectMSDL)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ENABLE_FLAT_PACKAGE)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/ThisIs395-VJ-ControlCenter.desktop"
        DESTINATION "${PROJECTMSDL_DESKTOP_DIR}" COMPONENT projectMSDL)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/ThisIs395-VJ-ControlCenter.svg"
        DESTINATION "${PROJECTMSDL_ICONS_DIR}/scalable/apps" COMPONENT projectMSDL)
endif()
