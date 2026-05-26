find_package(Qt6 REQUIRED COMPONENTS Test Gui)

qt_add_executable(test_pvj_roundtrip
    test_pvj_roundtrip.cpp
)

target_link_libraries(test_pvj_roundtrip
    PRIVATE
        pvj_core
        Qt6::Core
        Qt6::Test
)

add_test(NAME pvj_roundtrip COMMAND test_pvj_roundtrip)

qt_add_executable(test_video
    test_video.cpp
)

target_link_libraries(test_video
    PRIVATE
        pvj_video
        Qt6::Core
        Qt6::Test
)

add_test(NAME video_ffmpeg COMMAND test_video)

qt_add_executable(test_audio_ringbuffer
    test_audio_ringbuffer.cpp
)
target_link_libraries(test_audio_ringbuffer
    PRIVATE
        pvj_audio
        Qt6::Core
        Qt6::Test
)
add_test(NAME audio_ringbuffer COMMAND test_audio_ringbuffer)

qt_add_executable(test_shader_library
    test_shader_library.cpp
)

target_link_libraries(test_shader_library
    PRIVATE
        pvj_render
        Qt6::Gui
        Qt6::Test
)

add_test(NAME shader_library COMMAND test_shader_library)

qt_add_executable(test_input_mapping
    test_input_mapping.cpp
)

target_link_libraries(test_input_mapping
    PRIVATE
        pvj_input
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)

add_test(NAME input_mapping COMMAND test_input_mapping)

qt_add_executable(test_property_registry
    test_property_registry.cpp
)
target_link_libraries(test_property_registry
    PRIVATE
        pvj_core
        Qt6::Core
        Qt6::Test
)
add_test(NAME property_registry COMMAND test_property_registry)

qt_add_executable(test_filter_catalog
    test_filter_catalog.cpp
)
target_link_libraries(test_filter_catalog
    PRIVATE
        pvj_core
        Qt6::Core
        Qt6::Test
)
add_test(NAME filter_catalog COMMAND test_filter_catalog)

qt_add_executable(test_filter_param_schema
    test_filter_param_schema.cpp
)
target_link_libraries(test_filter_param_schema
    PRIVATE
        pvj_core
        Qt6::Core
        Qt6::Test
)
add_test(NAME filter_param_schema COMMAND test_filter_param_schema)

qt_add_executable(test_filter_effect_ids
    test_filter_effect_ids.cpp
)
target_link_libraries(test_filter_effect_ids
    PRIVATE
        pvj_core
        pvj_render
        Qt6::Gui
        Qt6::Test
)
add_test(NAME filter_effect_ids COMMAND test_filter_effect_ids)

qt_add_executable(test_maxine_catalog
    test_maxine_catalog.cpp
)
target_link_libraries(test_maxine_catalog
    PRIVATE
        pvj_core
        Qt6::Core
        Qt6::Test
)
add_test(NAME maxine_catalog COMMAND test_maxine_catalog)

qt_add_executable(test_filter_grid_scope
    test_filter_grid_scope.cpp
)
target_link_libraries(test_filter_grid_scope
    PRIVATE
        pvj_core
        Qt6::Core
        Qt6::Test
)
add_test(NAME filter_grid_scope COMMAND test_filter_grid_scope)

qt_add_executable(test_rhi_filter_chain
    test_rhi_filter_chain.cpp
)
target_link_libraries(test_rhi_filter_chain
    PRIVATE
        pvj_render
        Qt6::Gui
        Qt6::Widgets
        Qt6::Test
)
add_test(NAME rhi_filter_chain COMMAND test_rhi_filter_chain)

qt_add_executable(test_output_filter
    test_output_filter.cpp
)
target_link_libraries(test_output_filter
    PRIVATE
        pvj_core
        Qt6::Core
        Qt6::Test
)
add_test(NAME output_filter COMMAND test_output_filter)

if(VJ_FEATURE_DXV)
    qt_add_executable(test_hap_decode
        test_hap_decode.cpp
    )
    target_link_libraries(test_hap_decode
        PRIVATE
            pvj_video
            Qt6::Core
            Qt6::Test
    )
    add_test(NAME hap_decode COMMAND test_hap_decode)
endif()
