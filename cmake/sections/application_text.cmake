if(TARGET GUIForms::Application AND
   TARGET GUIForms::TextMasks AND GUI_FORMS_BUILD_AUDIO_LOOP_TRANSPORT)
    add_subdirectory(tests/application_text_consumer)
    # Reproduce the source consumer graph in a fresh CMake generation, retaining
    # the parent's compiler, pinned sources and platform renderer configuration.
    set(application_text_cache "${CMAKE_CURRENT_BINARY_DIR}/application-text-cache.cmake")
    file(WRITE "${application_text_cache}" "# Generated consumer configuration.\n")
    get_cmake_property(application_text_variables CACHE_VARIABLES)
    foreach(variable IN LISTS application_text_variables)
        if(variable MATCHES "^(GUI_FORMS_|FETCHCONTENT_SOURCE_DIR_|CMAKE_TOOLCHAIN_FILE$|CMAKE_MAKE_PROGRAM$|CMAKE_BUILD_TYPE$)")
            file(APPEND "${application_text_cache}"
                "set(${variable} [==[${${variable}}]==] CACHE STRING \"Consumer configuration\" FORCE)\n")
        endif()
    endforeach()
    # Both nested generations consume the already fetched, pinned source.
    # Otherwise each config test clones miniaudio into a fresh _deps directory.
    file(APPEND "${application_text_cache}"
        "set(FETCHCONTENT_SOURCE_DIR_GUI_FORMS_MINIAUDIO [==[${gui_forms_miniaudio_SOURCE_DIR}]==] CACHE PATH \"Pinned parent source\" FORCE)\n")
    # And they link the parent's stx package as is (cmake/Stx.cmake): given
    # only its directory, a nested generation whose flags differ would rebuild
    # the package in place, past the test's timeout.
    file(APPEND "${application_text_cache}"
        "set(GUI_FORMS_STX_PREFIX [==[${GUI_FORMS_STX_PACKAGE_ROOT}]==] CACHE PATH \"Parent stx package\" FORCE)\n")
    foreach(link_case IN ITEMS accepted)
        add_test(NAME gui_forms_application_text_configure_${link_case}
            COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/application_text_consumer"
                "-DPROVIDER=${CMAKE_CURRENT_SOURCE_DIR}"
                "-DBINARY=${CMAKE_CURRENT_BINARY_DIR}/application-text-${link_case}"
                "-DCACHE=${application_text_cache}"
                "-DLINK_CASE=${link_case}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_application_text.cmake")
        set_tests_properties(gui_forms_application_text_configure_${link_case}
            PROPERTIES TIMEOUT 180)
    endforeach()
endif()
