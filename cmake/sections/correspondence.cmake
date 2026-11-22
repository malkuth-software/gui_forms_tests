# Cut from gui_forms demo/correspondence/correspondence.cmake.
if(GUI_FORMS_BUILD_GALLERY)
    add_executable(gui_forms_correspondence_tests
        tests/correspondence_tests.cpp
    )
    target_link_libraries(gui_forms_correspondence_tests PRIVATE
        gui_forms_correspondence_model
    )
    add_test(NAME gui_forms_correspondence_tests
             COMMAND gui_forms_correspondence_tests)

    add_executable(gui_forms_correspondence_interaction_tests
        tests/correspondence_interaction_tests.cpp
    )
    target_link_libraries(gui_forms_correspondence_interaction_tests PRIVATE
        gui_forms_correspondence_model
    )
    add_test(NAME gui_forms_correspondence_interaction_tests
             COMMAND gui_forms_correspondence_interaction_tests)

    if(TARGET gui_forms_correspondence)
        add_test(
            NAME gui_forms_correspondence_font_policy
            COMMAND ${CMAKE_COMMAND}
                -DFONT_DIRECTORY=$<TARGET_FILE_DIR:gui_forms_correspondence>/../Resources/fonts
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_gallery_fonts.cmake
        )
    endif()
endif()
