if(TARGET gui_forms_gallery_model)
    if(GUI_FORMS_ENABLE_MACOS_HOST)
        # Live resize on the Gallery (cp33; tests/macos_live_resize_probe.mm):
        # a scripted corner drag and a stepped resize, recording each draw's
        # timing and damage; judged on stale damage, stalls and the final
        # frame. Its CSVs go to live-resize/ in the build tree; run it by hand
        # with --capture <dir> to save frames for the owner's verdict.
        add_executable(gui_forms_macos_live_resize_probe MACOSX_BUNDLE
            tests/macos_live_resize_probe.mm
        )
        target_compile_options(gui_forms_macos_live_resize_probe PRIVATE -fobjc-arc)
        target_link_libraries(gui_forms_macos_live_resize_probe PRIVATE
            gui_forms_gallery_model
            gui_forms_host_macos
        )
        set_target_properties(gui_forms_macos_live_resize_probe PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "local.guiforms.live-resize-probe")
        # Launched through LaunchServices so the window is active and
        # takes the posted drag (cmake/run_macos_activated_test.cmake).
        add_test(NAME gui_forms_macos_live_resize_probe_tests
                 COMMAND ${CMAKE_COMMAND}
                     -DBUNDLE=$<TARGET_BUNDLE_DIR:gui_forms_macos_live_resize_probe>
                     -DOUTPUT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_macos_activated_test.cmake)
        set_tests_properties(gui_forms_macos_live_resize_probe_tests PROPERTIES
            RUN_SERIAL TRUE TIMEOUT 150 LABELS "mac_conditions")
    endif()
endif()
