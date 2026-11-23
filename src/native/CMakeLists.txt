# Optional Qt6 control center built alongside the SDL/ImGui renderer.
# Both executables install in the same application's bin directory.
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)
add_executable(ThisIs395-VJ-ControlCenter WIN32
    NativeControlCenter.cpp
    NativeShowEditor.h
    NativeSpriteManager.h)
target_link_libraries(ThisIs395-VJ-ControlCenter PRIVATE Qt6::Widgets)
set_target_properties(ThisIs395-VJ-ControlCenter PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    OUTPUT_NAME "ThisIs395-VJ-ControlCenter"
)
install(TARGETS ThisIs395-VJ-ControlCenter
    RUNTIME DESTINATION "${PROJECTMSDL_BIN_DIR}"
    COMPONENT projectMSDL
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ENABLE_FLAT_PACKAGE)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/ThisIs395-VJ-ControlCenter.desktop"
        DESTINATION "${PROJECTMSDL_DESKTOP_DIR}" COMPONENT projectMSDL)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/ThisIs395-VJ-ControlCenter.svg"
        DESTINATION "${PROJECTMSDL_ICONS_DIR}/scalable/apps" COMPONENT projectMSDL)
endif()
