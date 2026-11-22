# Optional Qt6 control center; the SDL/ImGui renderer remains the existing application.
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)
add_executable(ThisIs395-VJ-ControlCenter WIN32 NativeControlCenter.cpp)
target_link_libraries(ThisIs395-VJ-ControlCenter PRIVATE Qt6::Widgets)
set_target_properties(ThisIs395-VJ-ControlCenter PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    OUTPUT_NAME "ThisIs395-VJ-ControlCenter"
)
install(TARGETS ThisIs395-VJ-ControlCenter
    RUNTIME DESTINATION "${PROJECTMSDL_BIN_DIR}"
)
