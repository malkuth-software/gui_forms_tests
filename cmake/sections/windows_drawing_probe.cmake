if(GUI_FORMS_ENABLE_WINDOWS_HOST)
    add_executable(gui_drawing_windows_probe
        tests/drawing_windows_probe.c
    )
    target_compile_definitions(gui_drawing_windows_probe PRIVATE
        ${GUI_FORMS_WINDOWS_DEFINITIONS}
    )
    target_link_libraries(gui_drawing_windows_probe PRIVATE
        gui_drawing_c_api gui_drawing_raster_absent gdi32 user32
    )
    add_test(NAME gui_drawing_windows_probe COMMAND gui_drawing_windows_probe)
    add_executable(gui_forms_windows_live_surface_endpoint_tests
        tests/windows_live_surface_endpoint_tests.cpp
    )
    target_compile_definitions(
        gui_forms_windows_live_surface_endpoint_tests PRIVATE
        ${GUI_FORMS_WINDOWS_DEFINITIONS}
    )
    target_link_libraries(gui_forms_windows_live_surface_endpoint_tests PRIVATE
        gui_forms_host_windows
    )
    add_test(NAME gui_forms_windows_live_surface_endpoint_tests
             COMMAND gui_forms_windows_live_surface_endpoint_tests)
    # The shim is part of the C ABI library (cmake/CAbi.cmake).
    if(GUI_FORMS_BUILD_C_ABI)
        add_executable(gui_forms_windows_gdi_compat_shim_tests
            tests/windows_gdi_compat_shim_tests.cpp
        )
        target_compile_definitions(
            gui_forms_windows_gdi_compat_shim_tests PRIVATE
            ${GUI_FORMS_WINDOWS_DEFINITIONS}
        )
        target_link_libraries(gui_forms_windows_gdi_compat_shim_tests PRIVATE
            gui_forms_abi gdi32 user32
        )
        add_test(NAME gui_forms_windows_gdi_compat_shim_tests
                 COMMAND gui_forms_windows_gdi_compat_shim_tests)
    endif()
endif()
