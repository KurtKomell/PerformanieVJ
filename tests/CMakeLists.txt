function(pvj_add_core_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE pvj_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

function(pvj_add_input_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE pvj_input pvj_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

pvj_add_core_test(test_vj2_import)
pvj_add_input_test(test_input_router)
