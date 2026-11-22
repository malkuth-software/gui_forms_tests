# The foreground and input-desktop tests (gui_forms_foreground_test and
# gui_forms_input_desktop_test, above), on the hosts that build them.
foreach(gui_forms_foreground_name IN ITEMS
        gui_forms_application_native_tests
        gui_forms_gallery_interaction_tests
        gui_forms_windows_accessibility_tests
        gui_forms_windows_dib_lifecycle_tests
        gui_forms_windows_input_method_tests
        gui_forms_windows_normative_trace
        gui_forms_windows_prepared_text_tests)
    if(TEST ${gui_forms_foreground_name})
        gui_forms_foreground_test(${gui_forms_foreground_name})
    endif()
endforeach()
# The Popup checks of the menu probe (the Gallery under Application::run),
# Python scripts run through the foreground launcher in front of the
# interpreter (TEST_LAUNCHER only reaches a test whose command is a target).
# They are registered only where a launcher is set, so only on a machine
# whose tests run on private desktops: tools/popup_margin_probe.py (Popup
# and tooltip window size and hit margin) and a short seeded smoke run of
# tools/contention_fuzz.py, labelled timing and non_gating (a long run is
# local and on demand; see the script). Both drive the probe through the
# library's MCP automation (--mcp-script, and --mcp with tools/mcp_client.py).
if(TARGET gui_forms_menu_probe_windows AND GUI_FORMS_MCP)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    set(gui_forms_popup_launcher "${GUI_FORMS_FOREGROUND_LAUNCHER}")
    if(NOT gui_forms_popup_launcher)
        set(gui_forms_popup_launcher "$ENV{GUI_FORMS_FOREGROUND_LAUNCHER}")
    endif()
    if(Python3_Interpreter_FOUND AND gui_forms_popup_launcher)
        add_test(NAME gui_forms_popup_margin_probe
            COMMAND ${gui_forms_popup_launcher} ${Python3_EXECUTABLE} -B
                ${GUI_FORMS_SOURCE_DIR}/tools/popup_margin_probe.py
                $<TARGET_FILE:gui_forms_menu_probe_windows> 1.5)
        set_property(TEST gui_forms_popup_margin_probe APPEND PROPERTY LABELS foreground)
        add_test(NAME gui_forms_contention_fuzz_smoke
            COMMAND ${gui_forms_popup_launcher} ${Python3_EXECUTABLE} -B
                ${GUI_FORMS_SOURCE_DIR}/tools/contention_fuzz.py
                $<TARGET_FILE:gui_forms_menu_probe_windows> --seed 20261110 --steps 40 --settle-every 10
                --no-shrink --out ${CMAKE_CURRENT_BINARY_DIR}/contention-fuzz)
        set_property(TEST gui_forms_contention_fuzz_smoke APPEND PROPERTY LABELS timing non_gating foreground)
    endif()
endif()
foreach(gui_forms_input_desktop_name IN ITEMS
        gui_forms_windows_accessibility_msaa_gallery_tests
        gui_forms_windows_accessibility_uia_gallery_tests
        gui_forms_windows_accessibility_uia_provider_tests
        gui_forms_windows_accessibility_uia_text_tests
        gui_forms_windows_compositor_probe)
    if(TEST ${gui_forms_input_desktop_name})
        gui_forms_input_desktop_test(${gui_forms_input_desktop_name})
    endif()
endforeach()
