if(GUI_FORMS_BUILD_FILE_MANAGER_DEMOBOARD)
    add_executable(gui_forms_file_manager_demoboard_tests
        file_manager_demoboard/tests/demoboard_tests.cpp)
    target_link_libraries(gui_forms_file_manager_demoboard_tests PRIVATE
        gui_forms_file_manager_demoboard_model)
    add_test(NAME gui_forms_file_manager_demoboard_tests
             COMMAND gui_forms_file_manager_demoboard_tests)
    add_test(NAME gui_forms_file_manager_demoboard_public_policy
        COMMAND ${CMAKE_COMMAND}
            -DDEMOBOARD_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/file_manager_demoboard
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_demoboard_boundaries.cmake)
endif()
