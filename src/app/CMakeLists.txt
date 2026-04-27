qt_add_executable(performanievj
    main.cpp
    MainWindow.cpp
    MainWindow.h
    BankGridWidget.cpp
    BankGridWidget.h
    FilterNodeEditorWindow.cpp
    FilterNodeEditorWindow.h
    NodeGraphWidget.cpp
    NodeGraphWidget.h
    MidiLearnMenu.cpp
    MidiLearnMenu.h
    MediaLibraryDock.cpp
    MediaLibraryDock.h
    ParameterInspector.cpp
    ParameterInspector.h
    PreferencesDialog.cpp
    PreferencesDialog.h
    PreviewWidget.cpp
    PreviewWidget.h
)

target_link_libraries(performanievj
    PRIVATE
        pvj_core
        pvj_video
        pvj_audio
        pvj_render
        pvj_input
        Qt6::Core
        Qt6::Concurrent
        Qt6::Gui
        Qt6::Widgets
)

# Windows: Qt must find platforms/qwindows*.dll next to the exe (or via QT_PLUGIN_PATH).
# qt_import_plugins() only registers targets; vcpkg puts the plugin Config under share/Qt6Gui,
# so we copy the imported DLL after each link (Debug/Release use different names).
if(WIN32)
    get_filename_component(_pvj_qt_gui_cmake "${Qt6_DIR}/../Qt6Gui" ABSOLUTE)
    find_package(Qt6QWindowsIntegrationPlugin REQUIRED CONFIG PATHS "${_pvj_qt_gui_cmake}" NO_DEFAULT_PATH)
    add_custom_command(
        TARGET performanievj
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:performanievj>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
            "$<TARGET_FILE_DIR:performanievj>/platforms/"
        VERBATIM
    )
endif()

target_include_directories(performanievj
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(performanievj PROPERTIES
    WIN32_EXECUTABLE  ON
    MACOSX_BUNDLE     ON
)

if(WIN32)
    target_compile_definitions(performanievj PRIVATE VJ_PLATFORM_WINDOWS=1)
elseif(APPLE)
    target_compile_definitions(performanievj PRIVATE VJ_PLATFORM_MACOS=1)
elseif(UNIX)
    target_compile_definitions(performanievj PRIVATE VJ_PLATFORM_LINUX=1)
endif()

if(VJ_FEATURE_DXV)
    target_compile_definitions(performanievj PRIVATE VJ_FEATURE_DXV=1)
endif()
if(VJ_FEATURE_SPOUT)
    target_compile_definitions(performanievj PRIVATE VJ_FEATURE_SPOUT=1)
endif()
if(VJ_FEATURE_SYPHON)
    target_compile_definitions(performanievj PRIVATE VJ_FEATURE_SYPHON=1)
endif()
if(VJ_FEATURE_NDI)
    target_compile_definitions(performanievj PRIVATE VJ_FEATURE_NDI=1)
endif()
if(VJ_FEATURE_ASIO)
    target_compile_definitions(performanievj PRIVATE VJ_FEATURE_ASIO=1)
endif()
