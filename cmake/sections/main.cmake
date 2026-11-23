enable_testing()
# The cached gui_forms_lamp_keys names its legend font by the absolute path of
# the tree that built it (GUI_FORMS_LEGEND_FONT, GUI.Forms' CI workspace), which
# no other machine has; the legend test builds the same program from the cache's
# source, as GUI.Forms does, naming the font the cache carries.
if(TARGET gui_forms_lamp_keys)
    add_executable(gui_forms_tests_lamp_keys
        "${GUI_FORMS_SOURCE_DIR}/tools/industrial_surfaces/lamp_keys.cpp"
        $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>)
    target_link_libraries(gui_forms_tests_lamp_keys PRIVATE gui_drawing_skia gui_forms_core)
    target_compile_definitions(gui_forms_tests_lamp_keys PRIVATE
        GUI_FORMS_LEGEND_FONT="${GUI_FORMS_SOURCE_DIR}/assets/fonts/PortsmouthMono.ttf")
    add_test(NAME gui_forms_lamp_keys_legend_tests COMMAND gui_forms_tests_lamp_keys --legend-test)
endif()
if(TARGET gui_forms_text_masks_impl)
    add_executable(gui_forms_text_mask_cost_probe EXCLUDE_FROM_ALL tests/text_mask_cost_probe.cpp)
    target_link_libraries(gui_forms_text_mask_cost_probe PRIVATE gui_forms_text_masks_impl)
    add_executable(gui_forms_text_mask_native_tests tests/text_mask_native_tests.cpp)
    target_link_libraries(gui_forms_text_mask_native_tests PRIVATE
        gui_forms_text_masks_impl stxlib::algorithms)
    add_test(NAME gui_forms_text_mask_native_tests
        COMMAND gui_forms_text_mask_native_tests "${GUI_FORMS_SOURCE_DIR}/assets/fonts"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/fonts/amiri/Amiri-Regular.ttf")
    set_tests_properties(gui_forms_text_mask_native_tests PROPERTIES TIMEOUT 30)
    add_executable(gui_forms_text_mask_wrapping_tests tests/text_mask_wrapping_tests.cpp)
    target_link_libraries(gui_forms_text_mask_wrapping_tests PRIVATE gui_forms_text_masks_impl)
    add_test(NAME gui_forms_text_mask_wrapping_tests
        COMMAND gui_forms_text_mask_wrapping_tests "${GUI_FORMS_SOURCE_DIR}/assets/fonts"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/fonts/amiri/Amiri-Regular.ttf")
    set_tests_properties(gui_forms_text_mask_wrapping_tests PROPERTIES TIMEOUT 30)
    add_executable(gui_forms_text_mask_lifecycle_tests tests/text_mask_lifecycle_tests.cpp)
    target_link_libraries(gui_forms_text_mask_lifecycle_tests PRIVATE gui_forms_text_masks_impl)
    add_test(NAME gui_forms_text_mask_lifecycle_tests
        COMMAND gui_forms_text_mask_lifecycle_tests "${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    set_tests_properties(gui_forms_text_mask_lifecycle_tests PROPERTIES TIMEOUT 30)
endif()
if(TARGET gui_forms_prepared_text_impl)
    add_executable(gui_forms_prepared_window_input_tests tests/prepared_window_input_tests.cpp)
    target_link_libraries(gui_forms_prepared_window_input_tests PRIVATE gui_forms_core)
    add_test(NAME gui_forms_prepared_window_input_tests COMMAND gui_forms_prepared_window_input_tests)
    add_executable(gui_forms_prepared_window_batch_tests tests/prepared_window_batch_tests.cpp)
    target_link_libraries(gui_forms_prepared_window_batch_tests PRIVATE gui_forms_core)
    add_test(NAME gui_forms_prepared_window_batch_tests COMMAND gui_forms_prepared_window_batch_tests)
    add_executable(gui_forms_prepared_window_shape_tests tests/prepared_window_shape_tests.cpp)
    target_link_libraries(gui_forms_prepared_window_shape_tests PRIVATE gui_forms_prepared_text_impl gui_forms_text_engine stxlib::algorithms)
    add_test(NAME gui_forms_prepared_window_shape_tests
        COMMAND gui_forms_prepared_window_shape_tests "${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    add_executable(gui_forms_prepared_window_session_tests tests/prepared_window_session_tests.cpp)
    target_link_libraries(gui_forms_prepared_window_session_tests PRIVATE gui_forms_prepared_text_impl gui_forms_text_engine)
    add_test(NAME gui_forms_prepared_window_session_tests
        COMMAND gui_forms_prepared_window_session_tests "${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    set_tests_properties(gui_forms_prepared_window_session_tests PROPERTIES TIMEOUT 60)
    add_executable(gui_forms_prepared_text_service_tests tests/prepared_text_service_tests.cpp)
    target_link_libraries(gui_forms_prepared_text_service_tests PRIVATE gui_forms_prepared_text_impl)
    add_test(NAME gui_forms_prepared_text_service_tests
        COMMAND gui_forms_prepared_text_service_tests "${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    add_executable(gui_forms_prepared_text_raster_tests tests/prepared_text_raster_tests.cpp)
    target_link_libraries(gui_forms_prepared_text_raster_tests PRIVATE gui_forms_prepared_text_impl)
    add_test(NAME gui_forms_prepared_text_raster_tests
        COMMAND gui_forms_prepared_text_raster_tests "${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    if(TARGET gui_forms_skia)
        add_executable(gui_forms_prepared_text_skia_tests tests/prepared_text_skia_tests.cpp)
        target_link_libraries(gui_forms_prepared_text_skia_tests PRIVATE
            gui_forms_skia gui_forms_prepared_text_impl stx_skia::stx_skia)
        add_test(NAME gui_forms_prepared_text_skia_tests
            COMMAND gui_forms_prepared_text_skia_tests "${GUI_FORMS_SOURCE_DIR}/assets/fonts")
        set_tests_properties(gui_forms_prepared_text_skia_tests PROPERTIES TIMEOUT 30)
    endif()
    add_executable(gui_forms_prepared_display_tests tests/prepared_display_tests.cpp)
    target_link_libraries(gui_forms_prepared_display_tests PRIVATE gui_forms_prepared_text_impl)
    add_test(NAME gui_forms_prepared_display_tests
        COMMAND gui_forms_prepared_display_tests "${GUI_FORMS_SOURCE_DIR}/assets/fonts")
endif()
add_executable(gui_forms_cursor_interaction_tests tests/cursor_interaction_tests.cpp)
target_link_libraries(gui_forms_cursor_interaction_tests PRIVATE gui_forms_core)
add_test(NAME cursor_interaction COMMAND gui_forms_cursor_interaction_tests)
add_executable(gui_forms_audio_loop_scheduler_tests
    tests/audio/audio_loop_scheduler_tests.cpp
    ${GUI_FORMS_SOURCE_DIR}/src/audio/loop_transport/scheduler.cpp)
target_include_directories(gui_forms_audio_loop_scheduler_tests PRIVATE ${GUI_FORMS_SOURCE_DIR}/src/audio/loop_transport)
target_compile_features(gui_forms_audio_loop_scheduler_tests PRIVATE cxx_std_20)
add_test(NAME gui_forms_audio_loop_scheduler_tests COMMAND gui_forms_audio_loop_scheduler_tests)
if(WIN32)
    add_executable(gui_forms_windows_message_queue_tests tests/windows_message_queue_tests.cpp)
    target_include_directories(gui_forms_windows_message_queue_tests PRIVATE ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/input)
    target_compile_features(gui_forms_windows_message_queue_tests PRIVATE cxx_std_20)
    target_link_libraries(gui_forms_windows_message_queue_tests PRIVATE user32)
    add_test(NAME gui_forms_windows_message_queue_tests COMMAND gui_forms_windows_message_queue_tests)
    # Explicit local probe only: needs ownership of an interactive desktop
    # window before injecting a bounded native key sequence. Not a CTest.
    add_executable(gui_forms_windows_message_queue_native_probe tests/windows_message_queue_native_probe.cpp)
    target_include_directories(gui_forms_windows_message_queue_native_probe PRIVATE ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/input)
    target_compile_features(gui_forms_windows_message_queue_native_probe PRIVATE cxx_std_20)
    target_link_libraries(gui_forms_windows_message_queue_native_probe PRIVATE user32)
    add_executable(gui_forms_windows_notify_icon_tests
        tests/windows_notify_icon_tests.cpp
        ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/services/windows_notify_icons.cpp)
    target_include_directories(gui_forms_windows_notify_icon_tests PRIVATE ${GUI_FORMS_SOURCE_DIR}/src/surface/windows)
    target_compile_definitions(gui_forms_windows_notify_icon_tests PRIVATE
        ${GUI_FORMS_WINDOWS_DEFINITIONS})
    target_link_libraries(gui_forms_windows_notify_icon_tests PRIVATE gui_forms_core user32 gdi32 shell32)
    add_test(NAME gui_forms_windows_notify_icon_tests COMMAND gui_forms_windows_notify_icon_tests)
    # OLE drag and drop with other programs: conversions, the drag data read
    # back as a destination reads it, and the drop-target registration.
    # Not built in a high-integrity configuration, which compiles none of it.
    if(NOT GUI_FORMS_HIGH_INTEGRITY)
        add_executable(gui_forms_windows_drag_drop_tests
            tests/windows_drag_drop_tests.cpp
            ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/services/windows_drag_drop.cpp)
        target_include_directories(gui_forms_windows_drag_drop_tests PRIVATE ${GUI_FORMS_SOURCE_DIR}/src/surface/windows)
        target_compile_definitions(gui_forms_windows_drag_drop_tests PRIVATE
            ${GUI_FORMS_WINDOWS_DEFINITIONS})
        target_link_libraries(gui_forms_windows_drag_drop_tests PRIVATE gui_forms_core user32 ole32 shell32 uuid)
        add_test(NAME gui_forms_windows_drag_drop_tests COMMAND gui_forms_windows_drag_drop_tests)
    endif()
            add_executable(gui_forms_windows_cursor_tests tests/windows_cursor_tests.cpp)
    target_link_libraries(gui_forms_windows_cursor_tests PRIVATE gui_forms_core user32 gdi32)
    add_test(NAME gui_forms_windows_cursors COMMAND gui_forms_windows_cursor_tests)
    add_executable(gui_forms_windows_key_translation_tests tests/windows_key_translation_tests.cpp)
    target_include_directories(gui_forms_windows_key_translation_tests PRIVATE ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/input)
    target_link_libraries(gui_forms_windows_key_translation_tests PRIVATE gui_forms_core)
    add_test(NAME gui_forms_windows_key_translation_tests COMMAND gui_forms_windows_key_translation_tests)
    add_executable(gui_forms_windows_dib_band_present_tests tests/windows_dib_band_present_tests.cpp)
    target_include_directories(gui_forms_windows_dib_band_present_tests PRIVATE ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/raster)
    target_compile_definitions(gui_forms_windows_dib_band_present_tests PRIVATE
        ${GUI_FORMS_WINDOWS_DEFINITIONS})
    target_link_libraries(gui_forms_windows_dib_band_present_tests PRIVATE gdi32 user32)
    add_test(NAME gui_forms_windows_dib_band_present COMMAND gui_forms_windows_dib_band_present_tests)
    if(GUI_FORMS_WINDOWS_TRANSACTIONAL_DIB)
        add_executable(gui_forms_windows_dib_frame_store_tests
            tests/windows_dib_frame_store_tests.cpp
            $<TARGET_OBJECTS:gui_forms_dib_frame_store_objects>)
        target_include_directories(gui_forms_windows_dib_frame_store_tests PRIVATE
            ${GUI_FORMS_SOURCE_DIR}/include ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/raster)
        target_compile_definitions(gui_forms_windows_dib_frame_store_tests PRIVATE
            ${GUI_FORMS_WINDOWS_DEFINITIONS}
            GUI_FORMS_WINDOWS_TRANSACTIONAL_DIB=1)
        target_link_libraries(gui_forms_windows_dib_frame_store_tests PRIVATE gdi32 user32)
        add_test(NAME gui_forms_windows_dib_frame_store_tests
                 COMMAND gui_forms_windows_dib_frame_store_tests)
        add_executable(gui_forms_windows_dib_lifecycle_tests
            tests/windows_dib_lifecycle_tests.cpp
            ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/application/windows_host.cpp
            $<TARGET_OBJECTS:gui_forms_windows_host_objects>
            $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>
            $<TARGET_OBJECTS:gui_forms_dib_frame_store_objects>)
        target_include_directories(gui_forms_windows_dib_lifecycle_tests PRIVATE
            ${GUI_FORMS_SOURCE_DIR}/include ${GUI_FORMS_SOURCE_DIR}/src/host/windows)
        target_compile_definitions(gui_forms_windows_dib_lifecycle_tests PRIVATE
            ${GUI_FORMS_WINDOWS_DEFINITIONS}
            GUI_FORMS_WINDOWS_TRANSACTIONAL_DIB=1 GUI_FORMS_DIB_LIFECYCLE_TEST=1)
        target_link_libraries(gui_forms_windows_dib_lifecycle_tests PRIVATE
            gui_forms_core gui_forms_skia gui_forms_text_engine user32 gdi32 imm32 ole32 oleaut32 oleacc uiautomationcore uuid
            comdlg32 shell32)
        add_test(NAME gui_forms_windows_dib_lifecycle_tests
                 COMMAND gui_forms_windows_dib_lifecycle_tests)
        set_tests_properties(gui_forms_windows_dib_lifecycle_tests PROPERTIES TIMEOUT 30)
        if(TARGET gui_forms_prepared_text_impl)
            add_executable(gui_forms_windows_prepared_text_tests
                tests/windows_prepared_text_tests.cpp
                ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/application/windows_host.cpp
                ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/fonts/sealed_pages_windows.cpp
                ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/accessibility/windows_accessibility.cpp
                ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/accessibility/windows_semantic_source.cpp
                ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/accessibility/windows_automation.cpp
                ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/accessibility/windows_text_automation.cpp
                ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/services/windows_clipboard_image.cpp
                ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/services/windows_notify_icons.cpp
                ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/locale/windows_date_pattern.cpp
                ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/locale/windows_locale.cpp
                ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/paint_endpoint/windows_compatibility_paint_endpoint.cpp
                ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/compositor/windows_compositor.cpp
                $<TARGET_OBJECTS:gui_forms_dib_frame_store_objects>)
            target_include_directories(gui_forms_windows_prepared_text_tests PRIVATE
                ${GUI_FORMS_SOURCE_DIR}/include ${GUI_FORMS_SOURCE_DIR}/src/host/windows)
            target_compile_definitions(gui_forms_windows_prepared_text_tests PRIVATE
                ${GUI_FORMS_WINDOWS_DEFINITIONS}
                GUI_FORMS_WINDOWS_TRANSACTIONAL_DIB=1
                GUI_FORMS_WINDOWS_PREPARED_TEXT=1 GUI_FORMS_WINDOWS_PREPARED_TEXT_TEST=1)
            target_link_libraries(gui_forms_windows_prepared_text_tests PRIVATE
                gui_forms_prepared_text_impl gui_forms_skia gui_forms_text_engine user32 gdi32 imm32 ole32 oleaut32 oleacc uiautomationcore uuid
                comdlg32 shell32)
            add_test(NAME gui_forms_windows_prepared_text_tests
                COMMAND gui_forms_windows_prepared_text_tests "${GUI_FORMS_SOURCE_DIR}/assets/fonts")
            set_tests_properties(gui_forms_windows_prepared_text_tests PROPERTIES TIMEOUT 30)
        endif()
    endif()
endif()
if(GUI_FORMS_ENABLE_LINUX_HOST)
    find_package(X11 QUIET)
    if(TARGET X11::Xcursor AND TARGET X11::Xfixes)
        add_executable(gui_forms_x11_cursor_tests tests/x11_cursor_tests.cpp)
        target_link_libraries(gui_forms_x11_cursor_tests PRIVATE gui_forms_core X11::Xcursor X11::Xfixes X11::X11)
        add_test(NAME gui_forms_x11_cursors COMMAND gui_forms_x11_cursor_tests)
    endif()
    # Stage T5, C4's measurement: what the X server and a compositing
    # manager do with premultiplied alpha, the 32-bit visual and the
    # _NET_WM_CM_Sn selection (XWayland included). Xlib, XFixes and XInput;
    # prints JSON lines (kept in CI's LastTest.log evidence) and fails
    # only when it cannot open the display. Serial: it reads the screen
    # and may start xcompmgr.
    if(TARGET X11::Xfixes AND TARGET X11::Xi)
        add_executable(gui_forms_x11_compositor_probe tests/x11_compositor_probe.cpp)
        target_link_libraries(gui_forms_x11_compositor_probe PRIVATE X11::Xfixes X11::Xi X11::X11)
        add_test(NAME gui_forms_x11_compositor_probe COMMAND gui_forms_x11_compositor_probe)
        set_tests_properties(gui_forms_x11_compositor_probe PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
        # The same probe under a headless Weston's Xwayland; it reports
        # the case not exercised where Weston is missing.
        add_test(NAME gui_forms_xwayland_compositor_probe
            COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/xwayland_compositor_probe.sh
                $<TARGET_FILE:gui_forms_x11_compositor_probe>)
        set_tests_properties(gui_forms_xwayland_compositor_probe PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)
    endif()
endif()
add_executable(gui_forms_cursor_image_tests tests/cursor_image_tests.cpp)
target_link_libraries(gui_forms_cursor_image_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_cursor_images COMMAND gui_forms_cursor_image_tests)

add_executable(gui_forms_application_contract_tests tests/application_contract_tests.cpp)
target_link_libraries(gui_forms_application_contract_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_application_contract_tests COMMAND gui_forms_application_contract_tests)
if(TARGET gui_forms_application)
    add_executable(gui_forms_application_native_tests tests/application_native_tests.cpp)
    target_link_libraries(gui_forms_application_native_tests PRIVATE gui_forms_application)
    if(GUI_FORMS_WINDOWS_AUTOMATION_ENVIRONMENT)
        # The test's accepted automation case runs only where the host honours the variable.
        target_compile_definitions(gui_forms_application_native_tests PRIVATE
            GUI_FORMS_WINDOWS_AUTOMATION_ENVIRONMENT=1)
    endif()
    add_test(NAME gui_forms_application_native_tests COMMAND gui_forms_application_native_tests)
    set_tests_properties(gui_forms_application_native_tests PROPERTIES TIMEOUT 30)
    # One stxlib_state in a program that reaches STXlib through the
    # Application archive and through its own stxlib::boundary link: the
    # static state archive is linked once, so a second state (two
    # allocators, two identity issuers) can only come from a duplicated
    # link, and fails it.
    add_executable(gui_forms_stxlib_state_sharing_tests tests/stxlib_state_sharing_tests.cpp)
    target_link_libraries(gui_forms_stxlib_state_sharing_tests PRIVATE gui_forms_application stxlib::boundary)
    add_test(NAME gui_forms_stxlib_state_sharing_tests COMMAND gui_forms_stxlib_state_sharing_tests)
    if(WIN32 AND GUI_FORMS_ENABLE_WINDOWS_HOST)
        # Stage T5, phase C2's gate: a non-opaque window on the Windows
        # host through each grant, read off the composed screen. Serial,
        # as on Linux and macOS: another test's window over the sampled
        # points changes the composed colour.
        add_executable(gui_forms_windows_compositor_tests tests/windows_compositor_tests.cpp)
        target_compile_definitions(gui_forms_windows_compositor_tests PRIVATE
            ${GUI_FORMS_WINDOWS_DEFINITIONS})
        target_link_libraries(gui_forms_windows_compositor_tests PRIVATE gui_forms_application dwmapi
            runtimeobject)
        add_test(NAME gui_forms_windows_compositor_tests COMMAND gui_forms_windows_compositor_tests)
        set_tests_properties(gui_forms_windows_compositor_tests PROPERTIES RUN_SERIAL TRUE TIMEOUT 60)
    endif()
    if(GUI_FORMS_ENABLE_LINUX_HOST)
        # Stage T5, phase C4's gate: a non-opaque window under Xvfb with no
        # compositing manager, then with xcompmgr started, stopped and
        # started again. Serial: it starts a compositor for the display.
        add_executable(gui_forms_linux_compositor_tests tests/linux_compositor_tests.cpp)
        target_link_libraries(gui_forms_linux_compositor_tests PRIVATE gui_forms_application X11::X11)
        add_test(NAME gui_forms_linux_compositor_tests COMMAND gui_forms_linux_compositor_tests)
        set_tests_properties(gui_forms_linux_compositor_tests PROPERTIES RUN_SERIAL TRUE TIMEOUT 60)
    endif()
    if(GUI_FORMS_ENABLE_MACOS_HOST)
        # Stage T5, phase C3's gate: a non-opaque window over a green
        # backdrop, read back from AppKit and off the composed screen.
        # Serial: it floats its windows over the desktop while it reads.
        add_executable(gui_forms_macos_compositor_tests tests/macos_compositor_tests.mm)
        target_compile_options(gui_forms_macos_compositor_tests PRIVATE -fobjc-arc)
        target_link_libraries(gui_forms_macos_compositor_tests PRIVATE gui_forms_application
            ${GUI_FORMS_APPKIT} ${GUI_FORMS_COREGRAPHICS})
        add_test(NAME gui_forms_macos_compositor_tests COMMAND gui_forms_macos_compositor_tests)
        set_tests_properties(gui_forms_macos_compositor_tests PROPERTIES RUN_SERIAL TRUE TIMEOUT 60)

        # Native AppKit readback of the window chrome, modal windows and
        # composite focus (Mac handoff 2026-11-09, item 2), and work that
        # must wake a modal session by itself (modal_wake_probe), and
        # click-through on an inactive window (first_click_probe). Launched
        # through LaunchServices so their windows can become key; serial,
        # since they need the key window. The focus probe writes its
        # captures to handoff_focus_captures/ in this directory.
        foreach(handoff_probe IN ITEMS window_chrome_probe modal_probe composite_focus_probe modal_wake_probe first_click_probe)
            set(handoff_target gui_forms_macos_${handoff_probe}_tests)
            add_executable(${handoff_target} MACOSX_BUNDLE tests/macos_${handoff_probe}_tests.mm
                ${GUI_FORMS_PROGRAM_FONT_NOTICES})
            target_compile_options(${handoff_target} PRIVATE -fobjc-arc)
            target_include_directories(${handoff_target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
            target_link_libraries(${handoff_target} PRIVATE gui_forms_application ${GUI_FORMS_APPKIT})
            add_test(NAME ${handoff_target}
                     COMMAND ${CMAKE_COMMAND}
                         -DBUNDLE=$<TARGET_BUNDLE_DIR:${handoff_target}>
                         -DOUTPUT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}
                         -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_macos_activated_test.cmake)
            set_tests_properties(${handoff_target} PROPERTIES RUN_SERIAL TRUE TIMEOUT 120)
        endforeach()
        # Files opened through application:openURLs: (Finder, the Dock),
        # sent as open-documents Apple Events by `open -a`, at launch and
        # during the run.
        add_executable(gui_forms_macos_open_files_probe_app MACOSX_BUNDLE
            tests/macos_open_files_probe_app.mm
            ${GUI_FORMS_PROGRAM_FONT_NOTICES})
        target_compile_options(gui_forms_macos_open_files_probe_app PRIVATE -fobjc-arc)
        target_include_directories(gui_forms_macos_open_files_probe_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        target_link_libraries(gui_forms_macos_open_files_probe_app PRIVATE gui_forms_application ${GUI_FORMS_APPKIT})
        set_target_properties(gui_forms_macos_open_files_probe_app PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "local.guiforms.open-files-probe")
        add_test(NAME gui_forms_macos_open_files_probe_tests
                 COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_macos_open_files_probe.sh
                     $<TARGET_BUNDLE_DIR:gui_forms_macos_open_files_probe_app>
                     ${CMAKE_CURRENT_BINARY_DIR})
        set_tests_properties(gui_forms_macos_open_files_probe_tests PROPERTIES RUN_SERIAL TRUE TIMEOUT 120)
        # Quit, logout and restart checks (Mac handoff 2026-11-09, item
        # 2e; cp29 row 14; cp33): a primary and a secondary window whose
        # closes are refused while gf_term_refuse and
        # gf_term_secondary_refuse exist in the user's temporary
        # directory (getconf DARWIN_USER_TEMP_DIR); it logs to
        # gf_term.log there.
        # The driver test types Cmd-Q inside the fixture (no permission
        # grant) and checks the order, the refusals, the saved state and
        # the run's return; logout and restart are started by hand with
        # `open -n -a`.
        add_executable(gui_forms_macos_termination_fixture MACOSX_BUNDLE
            tests/macos_termination_fixture.mm
            ${GUI_FORMS_PROGRAM_FONT_NOTICES})
        target_compile_options(gui_forms_macos_termination_fixture PRIVATE -fobjc-arc)
        target_link_libraries(gui_forms_macos_termination_fixture PRIVATE gui_forms_application ${GUI_FORMS_APPKIT})
        set_target_properties(gui_forms_macos_termination_fixture PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "local.guiforms.termination-fixture")
        add_test(NAME gui_forms_macos_termination_fixture_tests
                 COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_macos_termination_fixture.sh
                     $<TARGET_BUNDLE_DIR:gui_forms_macos_termination_fixture>)
        set_tests_properties(gui_forms_macos_termination_fixture_tests PROPERTIES
            RUN_SERIAL TRUE TIMEOUT 120 LABELS "mac_conditions")

        # The normative trace on macOS (tests/macos_normative_trace.mm),
        # launched through LaunchServices so the window is active and
        # takes the posted events; it writes normative/macos.trace beside
        # the bundle, then the comparison with the headless reference.
        add_executable(gui_forms_macos_normative_trace MACOSX_BUNDLE
            tests/macos_normative_trace.mm
            tests/support/normative_trace.cpp
            ${GUI_FORMS_GALLERY_FONTS} ${GUI_FORMS_GALLERY_FONT_NOTICES})
        target_compile_options(gui_forms_macos_normative_trace PRIVATE
            $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
        target_include_directories(gui_forms_macos_normative_trace PRIVATE tests)
        target_link_libraries(gui_forms_macos_normative_trace PRIVATE gui_forms_host_macos ${GUI_FORMS_APPKIT})
        set_target_properties(gui_forms_macos_normative_trace PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "local.guiforms.normative-trace")
        add_test(NAME gui_forms_macos_normative_trace
                 COMMAND ${CMAKE_COMMAND}
                     -DBUNDLE=$<TARGET_BUNDLE_DIR:gui_forms_macos_normative_trace>
                     -DOUTPUT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_macos_activated_test.cmake)
        set_tests_properties(gui_forms_macos_normative_trace PROPERTIES
            RUN_SERIAL TRUE TIMEOUT 90 FIXTURES_REQUIRED gui_forms_normative_reference
            FIXTURES_SETUP gui_forms_normative_macos LABELS "mac_conditions;normative_trace")
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            add_test(NAME gui_forms_macos_normative_trace_compare
                     COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tools/normative_trace.py compare
                         --reference-dir ${CMAKE_CURRENT_BINARY_DIR}/normative
                         --trace $<TARGET_BUNDLE_DIR:gui_forms_macos_normative_trace>/../normative/macos.trace)
            set_tests_properties(gui_forms_macos_normative_trace_compare PROPERTIES
                FIXTURES_REQUIRED "gui_forms_normative_reference;gui_forms_normative_macos"
                LABELS "mac_conditions;normative_trace")
        endif()
    endif()
    # Stage DK W0 measurement (notes:results/2026-10-18-dk-w0-capture-drag/): it
    # puts windows on the desktop, so it is built but never run by ctest.
    add_executable(gui_forms_frame_drag_probe tests/frame_drag_probe.cpp)
    target_link_libraries(gui_forms_frame_drag_probe PRIVATE gui_forms_application)
endif()
if(GUI_FORMS_ENABLE_LINUX_HOST)
    add_executable(gui_forms_linux_host_tests tests/linux_host_tests.cpp)
    target_link_libraries(gui_forms_linux_host_tests PRIVATE gui_forms_host_linux X11::X11)
    add_test(NAME gui_forms_linux_host_tests COMMAND gui_forms_linux_host_tests)
    set_tests_properties(gui_forms_linux_host_tests PROPERTIES TIMEOUT 45)
    # The normative trace on X11 (tests/linux_normative_trace.cpp), then
    # its comparison with the headless reference. Needs an X server.
    add_executable(gui_forms_linux_normative_trace
        tests/linux_normative_trace.cpp
        tests/support/normative_trace.cpp
    )
    target_include_directories(gui_forms_linux_normative_trace PRIVATE tests)
    target_link_libraries(gui_forms_linux_normative_trace PRIVATE gui_forms_host_linux X11::X11)
    add_test(NAME gui_forms_linux_normative_trace
             COMMAND gui_forms_linux_normative_trace
                 --output ${CMAKE_CURRENT_BINARY_DIR}/normative/x11.trace)
    set_tests_properties(gui_forms_linux_normative_trace PROPERTIES
        RUN_SERIAL TRUE TIMEOUT 60 FIXTURES_REQUIRED gui_forms_normative_reference
        FIXTURES_SETUP gui_forms_normative_x11 LABELS "normative_trace")
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME gui_forms_linux_normative_trace_compare
                 COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tools/normative_trace.py compare
                     --reference-dir ${CMAKE_CURRENT_BINARY_DIR}/normative
                     --trace ${CMAKE_CURRENT_BINARY_DIR}/normative/x11.trace)
        set_tests_properties(gui_forms_linux_normative_trace_compare PROPERTIES
            FIXTURES_REQUIRED "gui_forms_normative_reference;gui_forms_normative_x11"
            LABELS "normative_trace")
    endif()
    # Audit N66, option a: the host at Xft.dpi 192 under Xvfb, then at 96
    # and 144 during the run. Serial: it rewrites the display's
    # RESOURCE_MANAGER, which every other runtime on the display reads.
    add_executable(gui_forms_linux_hidpi_tests tests/linux_hidpi_tests.cpp)
    target_link_libraries(gui_forms_linux_hidpi_tests PRIVATE gui_forms_host_linux X11::X11)
    add_test(NAME gui_forms_linux_hidpi_tests COMMAND gui_forms_linux_hidpi_tests)
    set_tests_properties(gui_forms_linux_hidpi_tests PROPERTIES RUN_SERIAL TRUE TIMEOUT 45)
    if(TARGET gui_forms_prepared_text_impl)
        add_executable(gui_forms_linux_prepared_text_host_tests
            tests/linux_prepared_text_host_tests.cpp)
        target_link_libraries(gui_forms_linux_prepared_text_host_tests PRIVATE
            gui_forms_host_linux gui_forms_prepared_text_impl X11::X11)
        add_test(NAME gui_forms_linux_prepared_text_host_tests
            COMMAND gui_forms_linux_prepared_text_host_tests
                "${GUI_FORMS_SOURCE_DIR}/assets/fonts")
        set_tests_properties(gui_forms_linux_prepared_text_host_tests PROPERTIES TIMEOUT 45)
    endif()
endif()
add_library(gui_forms_reference_examples OBJECT
    ${GUI_FORMS_SOURCE_DIR}/examples/reference/clipboard_image.cpp
)
target_link_libraries(gui_forms_reference_examples PRIVATE gui_forms_core)
if(TARGET GUIForms::form_compiler)
    # The windowed references: each window is a form document (forms/),
    # generated at build time as examples/reference/CMakeLists.txt does.
    set(reference_forms ${GUI_FORMS_SOURCE_DIR}/examples/reference)
    add_library(gui_forms_reference_form_examples OBJECT
        ${reference_forms}/owned_events.cpp
        ${reference_forms}/bound_values.cpp
        ${reference_forms}/shared_state.cpp
        ${reference_forms}/collection_state.cpp
        ${reference_forms}/multiline_scrollbars.cpp
        ${reference_forms}/window_rules.cpp
        ${reference_forms}/mouse_events.cpp
        ${reference_forms}/collection_gallery.cpp)
    target_link_libraries(gui_forms_reference_form_examples PRIVATE gui_forms_core)
    target_include_directories(gui_forms_reference_form_examples PRIVATE
        ${GUI_FORMS_SOURCE_DIR}/examples/reference)
    gui_forms_add_form(gui_forms_reference_form_examples ${reference_forms}/forms/owned_events.form
        CLASS ExampleWindow NAMESPACE reference DERIVED
        USED_BY ${reference_forms}/owned_events.cpp)
    gui_forms_add_form(gui_forms_reference_form_examples ${reference_forms}/forms/bound_values.form
        CLASS SettingsWindow NAMESPACE reference DERIVED
        USED_BY ${reference_forms}/bound_values.cpp)
    gui_forms_add_form(gui_forms_reference_form_examples ${reference_forms}/forms/shared_state.form
        CLASS SharedWindow NAMESPACE reference
        MODEL reference::SharedModel MODEL_HEADER shared_state_model.hpp
        USED_BY ${reference_forms}/shared_state.cpp)
    gui_forms_add_form(gui_forms_reference_form_examples ${reference_forms}/forms/collection_state.form
        CLASS CollectionsWindow NAMESPACE reference DERIVED
        USED_BY ${reference_forms}/collection_state.cpp)
    gui_forms_add_form(gui_forms_reference_form_examples ${reference_forms}/forms/multiline_scrollbars.form
        CLASS ViewerWindow NAMESPACE reference DERIVED
        USED_BY ${reference_forms}/multiline_scrollbars.cpp)
    gui_forms_add_form(gui_forms_reference_form_examples ${reference_forms}/forms/window_rules.form
        CLASS RulesWindow NAMESPACE reference DERIVED
        USED_BY ${reference_forms}/window_rules.cpp)
    gui_forms_add_form(gui_forms_reference_form_examples ${reference_forms}/forms/mouse_events.form
        CLASS MouseWindow NAMESPACE reference DERIVED
        USED_BY ${reference_forms}/mouse_events.cpp)
    gui_forms_add_form(gui_forms_reference_form_examples ${reference_forms}/forms/collection_gallery.form
        CLASS GalleryWindow NAMESPACE reference
        MODEL reference::GalleryModel MODEL_HEADER collection_gallery_model.hpp
        USED_BY ${reference_forms}/collection_gallery.cpp)
    gui_forms_add_theme(gui_forms_reference_form_examples ${reference_forms}/themes/collection_gallery
        FUNCTION collection_gallery_theme NAMESPACE reference
        FORMS ${reference_forms}/forms/collection_gallery.form)
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
find_program(GUI_FORMS_REFERENCE_CLANG NAMES clang++)
if(Python3_Interpreter_FOUND AND GUI_FORMS_REFERENCE_CLANG)
    add_test(NAME gui_forms_owned_event_compile_failures
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/owned_event_compile_failures.py
            ${GUI_FORMS_REFERENCE_CLANG} ${GUI_FORMS_SOURCE_DIR}/include)
    add_test(NAME gui_forms_stable_id_compile_failures
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/stable_id_compile_failures.py
            ${GUI_FORMS_REFERENCE_CLANG} ${GUI_FORMS_SOURCE_DIR}/include)
    add_test(NAME gui_forms_api_reference_tests
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/api_reference_tests.py)
    # The include directories a consumer of the public headers compiles
    # with: gui_forms_core's own and those of its public dependencies
    # (stx_zlib, stx_png, ...), evaluated transitively by CMake, one per
    # line. tools/api_reference.py --build reads them from here.
    file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/api_reference_include_directories.txt
        CONTENT "$<JOIN:$<TARGET_PROPERTY:gui_forms_core,INTERFACE_INCLUDE_DIRECTORIES>,\n>\n")
    add_custom_target(gui_forms_api_reference
        COMMAND ${Python3_EXECUTABLE} ${GUI_FORMS_SOURCE_DIR}/tools/api_reference.py
            --clang ${GUI_FORMS_REFERENCE_CLANG}
            --build ${CMAKE_CURRENT_BINARY_DIR}
            --output ${CMAKE_CURRENT_BINARY_DIR}/reference
        WORKING_DIRECTORY ${GUI_FORMS_SOURCE_DIR}
        COMMENT "Extracting native C++ API and rendering authored contracts"
        VERBATIM)
endif()
# No test checks with bare assert(): a Release (NDEBUG) build compiles
# it out, and the test then checks nothing.
if(Python3_Interpreter_FOUND)
    add_test(NAME gui_forms_test_assert_check
        COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_test_asserts.py
            --root ${CMAKE_CURRENT_SOURCE_DIR})
    # The stock icon codec (tools/stock_icon_codec.py, the generator's
    # oracle): its SVG subset, the round trip of records, and the build's
    # generated records against the committed SVG files.
    add_test(NAME gui_forms_stock_icon_codec_tests
        COMMAND ${Python3_EXECUTABLE} -B -m unittest discover -s ${GUI_FORMS_SOURCE_DIR}/tools
            -p test_stock_icon_codec.py)
    set_tests_properties(gui_forms_stock_icon_codec_tests PROPERTIES
        ENVIRONMENT "GUI_FORMS_STOCK_ICON_DATA=${GUI_FORMS_STOCK_ICON_RECORDS}")
    # The oracle: the C++ generator's records are byte for byte what
    # the Python codec gives for the same SVG files.
    add_test(NAME gui_forms_stock_icon_generator_oracle
        COMMAND ${Python3_EXECUTABLE} -B ${GUI_FORMS_SOURCE_DIR}/tools/stock_icon_codec.py check
            --source ${GUI_FORMS_STOCK_ICON_SOURCE} --output ${GUI_FORMS_STOCK_ICON_RECORDS})
    add_test(NAME gui_forms_test_assert_check_unit
        COMMAND ${Python3_EXECUTABLE} -B -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tools
            -p test_check_test_asserts.py)
    # The C ABI's termination rule (tools/check_exit_statics.py): static
    # state in src/abi/ is never destroyed or trivially destructible.
    # The sensible-naming audit (tools/naming_audit.py): names that say
    # something other than what they hold. A report, not a gate.
    add_test(NAME gui_forms_naming_audit_unit
        COMMAND ${Python3_EXECUTABLE} -B -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tools
            -p test_naming_audit.py)
    # The ThreadSanitizer run's selection (tools/abi_tsan.py).
    add_test(NAME gui_forms_abi_tsan_selection_unit
        COMMAND ${Python3_EXECUTABLE} -B -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tools
            -p test_abi_tsan.py)
    add_test(NAME gui_forms_exit_statics_check_unit
        COMMAND ${Python3_EXECUTABLE} -B -m unittest discover -s ${GUI_FORMS_SOURCE_DIR}/tools
            -p test_check_exit_statics.py)
    # Stable IDs written in tests use no '_' (tools/check_stable_id_literals.py):
    # the C ABI's tests pass them as run-time strings the ABI refuses.
    add_test(NAME gui_forms_stable_id_literals_check_unit
        COMMAND ${Python3_EXECUTABLE} -B -m unittest discover -s ${GUI_FORMS_SOURCE_DIR}/tools
            -p test_check_stable_id_literals.py)
    # Every native entry (window procedure, COM method, ATK callback,
    # AppKit selector) runs through the fail-fast boundary, or is listed
    # in tools/native_boundary_baseline.txt, which may only shrink.
    add_test(NAME gui_forms_native_boundary_check
        COMMAND ${Python3_EXECUTABLE} -B ${GUI_FORMS_SOURCE_DIR}/tools/check_native_boundaries.py
            --root ${GUI_FORMS_SOURCE_DIR})
    add_test(NAME gui_forms_native_boundary_check_self_test
        COMMAND ${Python3_EXECUTABLE} -B ${GUI_FORMS_SOURCE_DIR}/tools/check_native_boundaries.py
            --self-test)
    # No consumer macro, known or not, can reach the consumer surface,
    # through the module or the shield (cmake/HostileMacroConformance.cmake).
    # Its objects build on request until they pass on every host.
    include(cmake/HostileMacroConformance.cmake)
    # The export lists under cmake/exports/ match the GUI_FORMS_API and
    # GUI_FORMS_THREADING_API marks, and every public declaration that
    # needs a mark has one (tools/export_annotations.py check, with its
    # glob self-test). A list that drifts from the marks fails here on
    # every host, Windows included, where the marks are dllexport.
    add_test(NAME gui_forms_export_lists_current
        COMMAND ${Python3_EXECUTABLE} -B ${GUI_FORMS_SOURCE_DIR}/tools/export_annotations.py check)
endif()
add_executable(gui_drawing_core_tests tests/drawing_core_tests.cpp)
target_link_libraries(gui_drawing_core_tests PRIVATE gui_drawing_core)
add_test(NAME gui_drawing_core_tests COMMAND gui_drawing_core_tests)
add_executable(gui_drawing_font_substitution_tests tests/drawing_font_substitution_tests.cpp)
target_link_libraries(gui_drawing_font_substitution_tests PRIVATE gui_drawing_core)
add_test(NAME gui_drawing_font_substitution_tests
         COMMAND gui_drawing_font_substitution_tests "${GUI_FORMS_SOURCE_DIR}/assets/fonts")

add_executable(gui_forms_canvas_tests tests/canvas_tests.cpp)
target_link_libraries(gui_forms_canvas_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_canvas_tests COMMAND gui_forms_canvas_tests)
add_executable(gui_forms_painter_text_fitting_tests tests/painter_text_fitting_tests.cpp)
target_link_libraries(gui_forms_painter_text_fitting_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_painter_text_fitting_tests COMMAND gui_forms_painter_text_fitting_tests)
add_executable(gui_forms_drawing_replay_tests tests/drawing_replay_tests.cpp)
target_link_libraries(gui_forms_drawing_replay_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_drawing_replay_tests COMMAND gui_forms_drawing_replay_tests)
if(GUI_FORMS_ENABLE_WINDOWS_HOST)
    add_executable(gui_forms_windows_control_snapshot_tests tests/windows_control_snapshot_tests.cpp)
    target_compile_definitions(gui_forms_windows_control_snapshot_tests PRIVATE
        ${GUI_FORMS_WINDOWS_DEFINITIONS})
    target_link_libraries(gui_forms_windows_control_snapshot_tests PRIVATE
        gui_forms_controls gui_forms_host_windows)
    add_test(NAME gui_forms_windows_control_snapshot_tests COMMAND gui_forms_windows_control_snapshot_tests)
    add_executable(gui_forms_windows_canvas_raster_tests tests/windows_canvas_raster_tests.cpp)
    target_compile_definitions(gui_forms_windows_canvas_raster_tests PRIVATE
        ${GUI_FORMS_WINDOWS_DEFINITIONS})
    target_link_libraries(gui_forms_windows_canvas_raster_tests PRIVATE
        gui_forms_controls gui_forms_host_windows)
    add_test(NAME gui_forms_windows_canvas_raster_tests COMMAND gui_forms_windows_canvas_raster_tests)
    add_executable(gui_forms_windows_damage_rectangles_tests tests/windows_damage_rectangles_tests.cpp)
    target_compile_definitions(gui_forms_windows_damage_rectangles_tests PRIVATE
        ${GUI_FORMS_WINDOWS_DEFINITIONS})
    target_link_libraries(gui_forms_windows_damage_rectangles_tests PRIVATE
        gui_forms_controls gui_forms_host_windows)
    add_test(NAME gui_forms_windows_damage_rectangles_tests
             COMMAND gui_forms_windows_damage_rectangles_tests)
    add_executable(gui_forms_windows_scroll_copy_tests tests/windows_scroll_copy_tests.cpp)
    target_compile_definitions(gui_forms_windows_scroll_copy_tests PRIVATE
        ${GUI_FORMS_WINDOWS_DEFINITIONS}
        GUI_FORMS_WINDOWS_SCROLL_COPY_TEST=1)
    target_link_libraries(gui_forms_windows_scroll_copy_tests PRIVATE
        gui_forms_controls gui_forms_host_windows)
    add_test(NAME gui_forms_windows_scroll_copy_tests
             COMMAND gui_forms_windows_scroll_copy_tests)
    add_executable(gui_forms_windows_live_idle_tests tests/windows_live_idle_tests.cpp)
    target_compile_definitions(gui_forms_windows_live_idle_tests PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00
        GUI_FORMS_WINDOWS_LIVE_IDLE_TEST=1)
    target_link_libraries(gui_forms_windows_live_idle_tests PRIVATE
        gui_forms_controls gui_forms_host_windows)
    add_test(NAME gui_forms_windows_live_idle_tests
             COMMAND gui_forms_windows_live_idle_tests)
    # Drops from other programs through the window's registered IDropTarget,
    # onto the Gallery's retained-rows collection (in process; it includes
    # windows_host.cpp, so it keeps its own program).
    if(TARGET gui_forms_gallery_model)
        add_executable(gui_forms_windows_drop_target_tests tests/windows_drop_target_tests.cpp)
        target_compile_definitions(gui_forms_windows_drop_target_tests PRIVATE
            ${GUI_FORMS_WINDOWS_DEFINITIONS}
            GUI_FORMS_WINDOWS_DROP_TEST=1)
        target_link_libraries(gui_forms_windows_drop_target_tests PRIVATE
            gui_forms_gallery_model gui_forms_controls gui_forms_host_windows)
        add_test(NAME gui_forms_windows_drop_target_tests
                 COMMAND gui_forms_windows_drop_target_tests)
    endif()
    add_executable(gui_forms_windows_schedule_wake_tests tests/windows_schedule_wake_tests.cpp)
    target_compile_definitions(gui_forms_windows_schedule_wake_tests PRIVATE
        ${GUI_FORMS_WINDOWS_DEFINITIONS}
        GUI_FORMS_WINDOWS_SCHEDULE_WAKE_TEST=1)
    target_link_libraries(gui_forms_windows_schedule_wake_tests PRIVATE
        gui_forms_controls gui_forms_host_windows)
    add_test(NAME gui_forms_windows_schedule_wake_tests
             COMMAND gui_forms_windows_schedule_wake_tests)
    add_executable(gui_forms_windows_message_box_tests tests/windows_message_box_tests.cpp)
    target_compile_definitions(gui_forms_windows_message_box_tests PRIVATE
        ${GUI_FORMS_WINDOWS_DEFINITIONS})
    target_link_libraries(gui_forms_windows_message_box_tests PRIVATE
        gui_forms_controls gui_forms_host_windows)
    add_test(NAME gui_forms_windows_message_box_tests
             COMMAND gui_forms_windows_message_box_tests)
    set_tests_properties(gui_forms_windows_message_box_tests PROPERTIES TIMEOUT 60)
    # Real message boxes, which activate: on a private desktop.
    gui_forms_foreground_test(gui_forms_windows_message_box_tests)
    add_executable(gui_forms_windows_text_tests tests/windows_text_tests.cpp)
    target_compile_definitions(gui_forms_windows_text_tests PRIVATE
        ${GUI_FORMS_WINDOWS_DEFINITIONS})
    target_link_libraries(gui_forms_windows_text_tests PRIVATE
        gui_forms_controls gui_forms_host_windows)
    add_test(NAME gui_forms_windows_text_tests COMMAND gui_forms_windows_text_tests
        "${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    add_executable(gui_forms_windows_backplane_replay_tests tests/windows_backplane_replay_tests.cpp)
    target_compile_definitions(gui_forms_windows_backplane_replay_tests PRIVATE
        ${GUI_FORMS_WINDOWS_DEFINITIONS})
    target_link_libraries(gui_forms_windows_backplane_replay_tests PRIVATE
        gui_forms_controls gui_forms_host_windows)
    add_test(NAME gui_forms_windows_backplane_replay_tests COMMAND gui_forms_windows_backplane_replay_tests
        "${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    # Stage T5, phase C2's measurement: what DWM composes from a
    # premultiplied client through each documented route. Prints JSON
    # lines (kept in CI's LastTest.log evidence); fails only when it
    # cannot make its windows or read the screen. Serial: it reads the
    # composed screen too.
    add_executable(gui_forms_windows_compositor_probe tests/windows_compositor_probe.cpp)
    target_compile_definitions(gui_forms_windows_compositor_probe PRIVATE
        ${GUI_FORMS_WINDOWS_DEFINITIONS})
    target_link_libraries(gui_forms_windows_compositor_probe PRIVATE dwmapi runtimeobject)
    add_test(NAME gui_forms_windows_compositor_probe COMMAND gui_forms_windows_compositor_probe)
    set_tests_properties(gui_forms_windows_compositor_probe PROPERTIES RUN_SERIAL TRUE)
    add_executable(gui_forms_windows_content_pixels_tests tests/windows_content_pixels_tests.cpp)
    target_compile_definitions(gui_forms_windows_content_pixels_tests PRIVATE
        ${GUI_FORMS_WINDOWS_DEFINITIONS})
    target_link_libraries(gui_forms_windows_content_pixels_tests PRIVATE
        gui_forms_controls gui_forms_host_windows)
    add_test(NAME gui_forms_windows_content_pixels_tests COMMAND gui_forms_windows_content_pixels_tests
        "${GUI_FORMS_SOURCE_DIR}/assets/fonts")

endif()


# The drawing library's per-thread last error (plain thread_local data).
add_executable(gui_drawing_last_error_tests tests/drawing_last_error_tests.cpp)
target_link_libraries(gui_drawing_last_error_tests PRIVATE gui_drawing_c_api gui_drawing_raster_absent)
add_test(NAME gui_drawing_last_error_tests COMMAND gui_drawing_last_error_tests)

add_executable(gui_drawing_c_api_c11_tests tests/drawing_c_api_c11_tests.c)
target_link_libraries(gui_drawing_c_api_c11_tests PRIVATE gui_drawing_c_api gui_drawing_raster_absent)
add_test(NAME gui_drawing_c_api_c11_tests COMMAND gui_drawing_c_api_c11_tests)

add_executable(gui_drawing_trace_equivalence_tests
    tests/drawing_trace_equivalence_tests.cpp
)
target_link_libraries(gui_drawing_trace_equivalence_tests PRIVATE
    gui_drawing_core
    gui_drawing_c_api
    gui_drawing_raster_absent
)
add_test(NAME gui_drawing_trace_equivalence_tests
         COMMAND gui_drawing_trace_equivalence_tests)

# The stx package stamp's digests ignore line endings
# (cmake/StxDigest.cmake, tests/cmake/stx_digest_test.cmake).
add_test(NAME gui_forms_stx_digest
         COMMAND ${CMAKE_COMMAND} -DWORK_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/stx_digest_test
                 -DGUI_FORMS_SOURCE_DIR=${GUI_FORMS_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/stx_digest_test.cmake)
# Public headers name no renderer or platform type; comments may. The check
# is gui_forms' own (its cmake/check_public_header_tokens.cmake, which its
# renderer boundary audit includes too). The fixtures prove a comment
# passes and a token in code fails.
add_test(NAME gui_forms_public_header_tokens
         COMMAND ${CMAKE_COMMAND} -DPUBLIC_INCLUDE_DIRECTORY=${GUI_FORMS_SOURCE_DIR}/include
                 -P ${GUI_FORMS_SOURCE_DIR}/cmake/check_public_header_tokens.cmake)
add_test(NAME gui_forms_public_header_tokens_comments_pass
         COMMAND ${CMAKE_COMMAND}
                 -DPUBLIC_INCLUDE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/public_header_tokens/comments_only
                 -P ${GUI_FORMS_SOURCE_DIR}/cmake/check_public_header_tokens.cmake)
add_test(NAME gui_forms_public_header_tokens_code_fails
         COMMAND ${CMAKE_COMMAND}
                 -DPUBLIC_INCLUDE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/public_header_tokens/in_code
                 -P ${GUI_FORMS_SOURCE_DIR}/cmake/check_public_header_tokens.cmake)
set_tests_properties(gui_forms_public_header_tokens_code_fails PROPERTIES
    PASS_REGULAR_EXPRESSION "exposes renderer/platform token: CGContext")

add_test(
    NAME gui_drawing_renderer_boundary_audit
    COMMAND ${CMAKE_COMMAND}
        -DDRAWING_CORE=$<TARGET_FILE:gui_drawing_core>
        -DDRAWING_C_API=$<TARGET_FILE:gui_drawing_c_api_c11_tests>
        -DDRAWING_TEST_BINARY=$<TARGET_FILE:gui_drawing_core_tests>
        -DSOURCE_DIRECTORY=${GUI_FORMS_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_drawing_boundaries.cmake
)
# The drawing ABI's export policy is the C ABI library's
# (gui_forms_abi_exports_only_entry_points, cmake/CAbi.cmake): it has no
# shared library of its own.

add_executable(gui_forms_host_image_tests tests/host_image_tests.cpp)
target_link_libraries(gui_forms_host_image_tests PRIVATE gui_forms_host_headless)
add_test(NAME gui_forms_host_image_tests COMMAND gui_forms_host_image_tests)

# The deflate contract harness's standalone inputs, in the normal suite.
gui_forms_add_fuzz_test(deflate SOURCE ${GUI_FORMS_SOURCE_DIR}/fuzz/deflate_fuzz.cpp LIBRARIES stx_zlib::stx_zlib)

add_executable(gui_forms_inflate_protocol_tests ${GUI_FORMS_SOURCE_DIR}/fuzz/inflate_protocol_fuzz.cpp)
target_link_libraries(gui_forms_inflate_protocol_tests PRIVATE gui_forms_core)
target_compile_definitions(gui_forms_inflate_protocol_tests PRIVATE GUI_FORMS_FUZZ_STANDALONE=1)
add_test(NAME gui_forms_inflate_protocol_tests COMMAND gui_forms_inflate_protocol_tests)

add_executable(gui_forms_png_pixels_tests tests/image/png_pixels_tests.cpp)
target_link_libraries(gui_forms_png_pixels_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_png_pixels_tests COMMAND gui_forms_png_pixels_tests)

# The codec-neutral resource seam: admission and decoding.
add_executable(gui_forms_resource_pixels_tests tests/image/resource_pixels_tests.cpp)
target_link_libraries(gui_forms_resource_pixels_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_resource_pixels_tests COMMAND gui_forms_resource_pixels_tests)
# A timing probe for audit P12, built on demand and never a test.
add_executable(gui_forms_png_decode_probe EXCLUDE_FROM_ALL tests/image/png_decode_probe.cpp)
target_link_libraries(gui_forms_png_decode_probe PRIVATE gui_forms_core)

# Deterministic malformed-input sweeps (owner revision of decision 6A).
add_executable(gui_forms_malformed_sweep_tests tests/compression/malformed_sweep_tests.cpp)
target_link_libraries(gui_forms_malformed_sweep_tests PRIVATE stx_zlib::stx_zlib)
add_test(NAME gui_forms_malformed_sweep_tests COMMAND gui_forms_malformed_sweep_tests)
add_executable(gui_forms_png_malformed_sweep_tests tests/image/png_malformed_sweep_tests.cpp)
target_link_libraries(gui_forms_png_malformed_sweep_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_png_malformed_sweep_tests COMMAND gui_forms_png_malformed_sweep_tests)

add_executable(gui_forms_png_color_tests tests/image/png_color_tests.cpp)
target_link_libraries(gui_forms_png_color_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_png_color_tests COMMAND gui_forms_png_color_tests)

# The internal interpolation seam over stxlib::conv (binary64, clamped).
add_executable(gui_forms_interpolation_tests tests/interpolation/interpolation_tests.cpp)
target_link_libraries(gui_forms_interpolation_tests PRIVATE gui_forms_core stxlib::algorithms)
add_test(NAME gui_forms_interpolation_tests COMMAND gui_forms_interpolation_tests)

# By-hand measurement of encoder size and time per level; not a test.
add_executable(gui_forms_png_encode_benchmark tests/image/png_encode_benchmark.cpp)
target_link_libraries(gui_forms_png_encode_benchmark PRIVATE gui_forms_core)

add_executable(gui_forms_png_writer_tests tests/png_writer_tests.cpp)
target_link_libraries(gui_forms_png_writer_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_png_writer_tests COMMAND gui_forms_png_writer_tests)

add_executable(gui_forms_core_tests tests/core_tests.cpp)
target_link_libraries(gui_forms_core_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_core_tests COMMAND gui_forms_core_tests)

add_executable(gui_forms_page_set_tests tests/page_set_tests.cpp)
target_link_libraries(gui_forms_page_set_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_page_set_tests COMMAND gui_forms_page_set_tests)

add_executable(gui_forms_engine_raster_cache_tests tests/engine_raster_cache_tests.cpp)
target_link_libraries(gui_forms_engine_raster_cache_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_engine_raster_cache_tests COMMAND gui_forms_engine_raster_cache_tests)
add_executable(gui_forms_engine_part_table_tests tests/engine_part_table_tests.cpp)
target_link_libraries(gui_forms_engine_part_table_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_engine_part_table_tests COMMAND gui_forms_engine_part_table_tests)
add_executable(gui_forms_part_painting_tests tests/part_painting_tests.cpp)
target_link_libraries(gui_forms_part_painting_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_part_painting_tests COMMAND gui_forms_part_painting_tests)
add_executable(gui_forms_theme_contrast_tests tests/theme_contrast_tests.cpp)
target_link_libraries(gui_forms_theme_contrast_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_theme_contrast_tests COMMAND gui_forms_theme_contrast_tests)
add_executable(gui_forms_dark_variant_tests tests/dark_variant_tests.cpp)
target_link_libraries(gui_forms_dark_variant_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_dark_variant_tests COMMAND gui_forms_dark_variant_tests)
add_executable(gui_forms_gui_forms_theme_tests tests/gui_forms_theme_tests.cpp)
target_link_libraries(gui_forms_gui_forms_theme_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_gui_forms_theme_tests COMMAND gui_forms_gui_forms_theme_tests)
add_executable(gui_forms_familiar_themes_tests tests/familiar_themes_tests.cpp)
target_link_libraries(gui_forms_familiar_themes_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_familiar_themes_tests COMMAND gui_forms_familiar_themes_tests)
add_executable(gui_forms_owner_draw_tests tests/owner_draw_tests.cpp)
target_link_libraries(gui_forms_owner_draw_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_owner_draw_tests COMMAND gui_forms_owner_draw_tests)
add_executable(gui_forms_theme_high_contrast_tests tests/theme_high_contrast_tests.cpp)
target_link_libraries(gui_forms_theme_high_contrast_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_theme_high_contrast_tests COMMAND gui_forms_theme_high_contrast_tests)
add_executable(gui_forms_theme_cascade_tests tests/theme_cascade_tests.cpp)
target_link_libraries(gui_forms_theme_cascade_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_theme_cascade_tests COMMAND gui_forms_theme_cascade_tests)
add_executable(gui_forms_theme_user_layer_tests tests/theme_user_layer_tests.cpp)
target_link_libraries(gui_forms_theme_user_layer_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_theme_user_layer_tests COMMAND gui_forms_theme_user_layer_tests)

add_executable(gui_forms_theme_user_fill_selection_tests tests/theme_user_fill_selection_tests.cpp)
target_link_libraries(gui_forms_theme_user_fill_selection_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_user_fill_selection_tests COMMAND gui_forms_theme_user_fill_selection_tests)

add_executable(gui_forms_effect_program_tests tests/effect_program_tests.cpp)
target_link_libraries(gui_forms_effect_program_tests PRIVATE gui_forms_controls)
if(GUI_FORMS_EFFECTS_VERIFIED)
    target_compile_definitions(gui_forms_effect_program_tests PRIVATE GUI_FORMS_EFFECT_TEST_VERIFIED)
endif()
add_test(NAME gui_forms_effect_program_tests COMMAND gui_forms_effect_program_tests)
# The effect pass itself: each case under tests/effect_pass is an object
# library outside `all`, compiled by its test with the plugin (or, for
# `unverified`, without it), its diagnostics matched against the
# `// expect:` line at the head of the case. The tests run the build tool
# in this tree, so they take the tree's lock one at a time.
if(GUI_FORMS_EFFECTS_VERIFIED)
    set(_gf_effect_cases
        passes while_loop unbounded_for loop_variable_written not_equal_past_limit not_equal_below_limit
        recursion virtual_call function_pointer
        allocation global_write cmath_call opaque_call pointer_state oversized_state over_budget unverified
        signal_passes signal_over_budget signal_cmath_call)
    foreach(_gf_case IN LISTS _gf_effect_cases)
        set(_gf_case_target gui_forms_effect_case_${_gf_case})
        add_library(${_gf_case_target} OBJECT EXCLUDE_FROM_ALL tests/effect_pass/${_gf_case}.cpp)
        # A case is deliberately wrong code whose diagnostic the test reads
        # (and `unverified` expects a #warning): warnings stay warnings here,
        # or -Werror would answer first. A source property, so it comes after
        # the targets' -Werror on the command line.
        set_source_files_properties(tests/effect_pass/${_gf_case}.cpp PROPERTIES COMPILE_OPTIONS -Wno-error)
        # For the usage requirements only: with Ninja an object library's
        # compiles do not wait for the libraries it links.
        target_link_libraries(${_gf_case_target} PRIVATE gui_forms_controls)
        if(_gf_case STREQUAL "unverified")
            target_compile_definitions(${_gf_case_target} PRIVATE GUI_FORMS_EFFECTS_UNVERIFIED)
        else()
            gui_forms_compile_with_localizable_plugin(${_gf_case_target})
        endif()
        add_test(NAME gui_forms_effect_pass_${_gf_case}
                 COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${CMAKE_BINARY_DIR} -DTARGET=${_gf_case_target}
                         -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/effect_pass/${_gf_case}.cpp
                         -DOBJECT_DIR=CMakeFiles/${_gf_case_target}.dir
                         -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_effect_pass.cmake)
        set_tests_properties(gui_forms_effect_pass_${_gf_case} PROPERTIES RESOURCE_LOCK gui_forms_build_tree)
    endforeach()
endif()
add_executable(gui_forms_motion_clock_tests tests/motion_clock_tests.cpp)
target_link_libraries(gui_forms_motion_clock_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_motion_clock_tests COMMAND gui_forms_motion_clock_tests)
add_executable(gui_forms_linear_light_tests tests/linear_light_tests.cpp)
target_link_libraries(gui_forms_linear_light_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_linear_light_tests COMMAND gui_forms_linear_light_tests)
add_executable(gui_forms_engine_math_tests tests/engine_math_tests.cpp)
target_link_libraries(gui_forms_engine_math_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_engine_math_tests COMMAND gui_forms_engine_math_tests)
add_executable(gui_forms_engine_expression_tests tests/engine_expression_tests.cpp)
target_link_libraries(gui_forms_engine_expression_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_engine_expression_tests COMMAND gui_forms_engine_expression_tests)
add_executable(gui_forms_engine_input_tests tests/engine_input_tests.cpp)
target_link_libraries(gui_forms_engine_input_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_input_tests COMMAND gui_forms_engine_input_tests)
add_executable(gui_forms_engine_motion_tests tests/engine_motion_tests.cpp)
target_link_libraries(gui_forms_engine_motion_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_motion_tests COMMAND gui_forms_engine_motion_tests)
add_executable(gui_forms_engine_primitive_tests tests/engine_primitive_tests.cpp)
target_link_libraries(gui_forms_engine_primitive_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_engine_primitive_tests COMMAND gui_forms_engine_primitive_tests)
add_executable(gui_forms_engine_relief_tests tests/engine_relief_tests.cpp)
target_link_libraries(gui_forms_engine_relief_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_relief_tests COMMAND gui_forms_engine_relief_tests)
add_executable(gui_forms_engine_scale_slot_tests tests/engine_scale_slot_tests.cpp)
target_link_libraries(gui_forms_engine_scale_slot_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_scale_slot_tests COMMAND gui_forms_engine_scale_slot_tests)
add_executable(gui_forms_engine_motion_curve_tests tests/engine_motion_curve_tests.cpp)
target_link_libraries(gui_forms_engine_motion_curve_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_motion_curve_tests COMMAND gui_forms_engine_motion_curve_tests)
add_executable(gui_forms_engine_material_tests tests/engine_material_tests.cpp)
target_link_libraries(gui_forms_engine_material_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_material_tests COMMAND gui_forms_engine_material_tests)
add_executable(gui_forms_engine_authored_material_tests tests/engine_authored_material_tests.cpp)
target_link_libraries(gui_forms_engine_authored_material_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_authored_material_tests COMMAND gui_forms_engine_authored_material_tests)
add_executable(gui_forms_engine_recipe_field_tests tests/engine_recipe_field_tests.cpp)
target_link_libraries(gui_forms_engine_recipe_field_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_recipe_field_tests COMMAND gui_forms_engine_recipe_field_tests)
add_executable(gui_forms_engine_procedural_tests tests/engine_procedural_tests.cpp)
target_link_libraries(gui_forms_engine_procedural_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_procedural_tests COMMAND gui_forms_engine_procedural_tests)
# Surface finishes (skeudoindustrial surfaces, phase 1): the kernel's
# bytes against the JavaScript reference, damage invariance, the
# material layer and finish(); --measure and --preview <dir>.
add_executable(gui_forms_engine_finish_tests tests/engine_finish_tests.cpp)
target_link_libraries(gui_forms_engine_finish_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_finish_tests COMMAND gui_forms_engine_finish_tests)
add_executable(gui_forms_engine_finish_tile_tests tests/engine_finish_tile_tests.cpp)
target_link_libraries(gui_forms_engine_finish_tile_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_finish_tile_tests COMMAND gui_forms_engine_finish_tile_tests)
# Gradient dithering (MaterialDither): the tables, banding metrics,
# determinism, damage invariance at scales 1 to 3, no swimming under a
# scroll, slices and spans keeping the pattern, gradient-dither and the
# metallic-fade recipe; --preview <dir> writes the fade as PNG files.
add_executable(gui_forms_engine_dither_tests tests/engine_dither_tests.cpp)
target_link_libraries(gui_forms_engine_dither_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_dither_tests COMMAND gui_forms_engine_dither_tests)
# Lamps and lit keys as baked light planes (phase 3): the bake file,
# the composite, the envelopes and the cache; --preview <out> <bakes>.
add_executable(gui_forms_engine_light_planes_tests tests/engine_light_planes_tests.cpp)
target_link_libraries(gui_forms_engine_light_planes_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_light_planes_tests COMMAND gui_forms_engine_light_planes_tests)
add_executable(gui_forms_engine_slice_tests tests/engine_slice_tests.cpp)
target_link_libraries(gui_forms_engine_slice_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_slice_tests COMMAND gui_forms_engine_slice_tests)

add_executable(gui_forms_engine_retained_tests tests/engine_retained_tests.cpp)
target_link_libraries(gui_forms_engine_retained_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_retained_tests COMMAND gui_forms_engine_retained_tests)

add_executable(gui_forms_engine_glass_tests tests/engine_glass_tests.cpp)
target_link_libraries(gui_forms_engine_glass_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_glass_tests COMMAND gui_forms_engine_glass_tests)
add_executable(gui_forms_engine_backdrop_tests tests/engine_backdrop_tests.cpp)
target_link_libraries(gui_forms_engine_backdrop_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_backdrop_tests COMMAND gui_forms_engine_backdrop_tests)
add_executable(gui_forms_engine_reflection_tests tests/engine_reflection_tests.cpp)
target_link_libraries(gui_forms_engine_reflection_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_reflection_tests COMMAND gui_forms_engine_reflection_tests)
add_executable(gui_forms_engine_backplane_replay_tests tests/engine_backplane_replay_tests.cpp)
target_link_libraries(gui_forms_engine_backplane_replay_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_backplane_replay_tests COMMAND gui_forms_engine_backplane_replay_tests)
add_executable(gui_forms_engine_light_tests tests/engine_light_tests.cpp)
target_link_libraries(gui_forms_engine_light_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_light_tests COMMAND gui_forms_engine_light_tests)
add_executable(gui_forms_engine_pop_tests tests/engine_pop_tests.cpp)
target_link_libraries(gui_forms_engine_pop_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_pop_tests COMMAND gui_forms_engine_pop_tests)

# T2's gate evidence (E11): every registered primitive's and every feature
# sample's reference images, the census of features, and the idle-frame
# counters, written as a manifest that CI compares across hosts
# (tools/engine_gate.py).
add_executable(gui_forms_engine_gate_tests tests/engine_gate_tests.cpp)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # The census names every value of the engine's vocabulary in a switch:
    # a value added without a name (and a feature sample) fails the build.
    set_source_files_properties(tests/engine_gate_tests.cpp PROPERTIES COMPILE_OPTIONS "-Werror=switch")
endif()
target_link_libraries(gui_forms_engine_gate_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_gate_tests COMMAND gui_forms_engine_gate_tests)

add_executable(gui_forms_sha256_tests tests/sha256_tests.cpp)
target_link_libraries(gui_forms_sha256_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_sha256_tests COMMAND gui_forms_sha256_tests)

# The bundled font set over the platform's sealed pages (item 4, step 3); the fault mode
# must end in the platform's protection fault (cmake/expect_fault.cmake).
add_executable(gui_forms_bundled_font_set_tests
    tests/bundled_font_set_tests.cpp $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>)
target_link_libraries(gui_forms_bundled_font_set_tests PRIVATE gui_forms_core)
# The pack tests read the container's index to alter a stored CRC-32; the loader's objects
# are in Core.
target_include_directories(gui_forms_bundled_font_set_tests PRIVATE
    ${GUI_FORMS_SOURCE_DIR}/stx_fontpack/include)
target_compile_definitions(gui_forms_bundled_font_set_tests PRIVATE
    GUI_FORMS_TEST_FONT_DIRECTORY="${GUI_FORMS_SOURCE_DIR}/assets/fonts")
add_test(NAME gui_forms_bundled_font_set_tests COMMAND gui_forms_bundled_font_set_tests)
add_test(NAME gui_forms_bundled_font_set_write_faults
         COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:gui_forms_bundled_font_set_tests>
                 -DMODE=--write-sealed
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/expect_fault.cmake)

add_executable(gui_forms_engine_fill_shape_tests tests/engine_fill_shape_tests.cpp)
target_link_libraries(gui_forms_engine_fill_shape_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_fill_shape_tests COMMAND gui_forms_engine_fill_shape_tests)
add_executable(gui_forms_engine_filter_blend_tests tests/engine_filter_blend_tests.cpp)
target_link_libraries(gui_forms_engine_filter_blend_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_engine_filter_blend_tests COMMAND gui_forms_engine_filter_blend_tests)

add_executable(gui_forms_damage_clip_tests tests/damage_clip_tests.cpp)
target_link_libraries(gui_forms_damage_clip_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_damage_clip_tests COMMAND gui_forms_damage_clip_tests)
add_executable(gui_forms_invalidation_overlay_tests tests/invalidation_overlay_tests.cpp)
target_link_libraries(gui_forms_invalidation_overlay_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_invalidation_overlay_tests COMMAND gui_forms_invalidation_overlay_tests)
add_executable(gui_forms_blend_study_tests tests/blend_study_tests.cpp)
target_link_libraries(gui_forms_blend_study_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_blend_study_tests COMMAND gui_forms_blend_study_tests)
add_executable(gui_forms_business_card_tests tests/business_card_tests.cpp)
target_include_directories(gui_forms_business_card_tests PRIVATE tests)
target_link_libraries(gui_forms_business_card_tests PRIVATE gui_forms_host_headless)
add_test(NAME gui_forms_business_card_tests COMMAND gui_forms_business_card_tests)

add_executable(gui_forms_control_table_tests tests/control_table_tests.cpp)
target_link_libraries(gui_forms_control_table_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_control_table_tests COMMAND gui_forms_control_table_tests)
add_executable(gui_forms_tree_cost_tests tests/tree_cost_tests.cpp)
target_link_libraries(gui_forms_tree_cost_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_tree_cost_tests COMMAND gui_forms_tree_cost_tests)
add_executable(gui_forms_control_core_tests tests/control_core_tests.cpp)
target_link_libraries(gui_forms_control_core_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_control_core_tests COMMAND gui_forms_control_core_tests)
add_executable(gui_forms_window_correctness_tests tests/window_correctness_tests.cpp)
target_link_libraries(gui_forms_window_correctness_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_window_correctness_tests COMMAND gui_forms_window_correctness_tests)
add_executable(gui_forms_property_capability_tests tests/property_capability_tests.cpp)
target_link_libraries(gui_forms_property_capability_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_property_capability_tests COMMAND gui_forms_property_capability_tests)
# Written by tools/property_codemod.py tests from tools/property_families/.
add_executable(gui_forms_property_family_generated_tests tests/property_family_generated_tests.cpp)
target_link_libraries(gui_forms_property_family_generated_tests PRIVATE gui_forms_lab gui_forms_demo_controls)
add_test(NAME gui_forms_property_family_generated_tests COMMAND gui_forms_property_family_generated_tests)

add_executable(gui_forms_damage_rectangles_tests tests/damage_rectangles_tests.cpp)
target_link_libraries(gui_forms_damage_rectangles_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_damage_rectangles_tests COMMAND gui_forms_damage_rectangles_tests)

add_executable(gui_forms_binary_search_tests tests/binary_search_tests.cpp)
target_link_libraries(gui_forms_binary_search_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_binary_search_tests COMMAND gui_forms_binary_search_tests)

add_executable(gui_forms_sort_algorithm_tests tests/sort_algorithm_tests.cpp)
target_link_libraries(gui_forms_sort_algorithm_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_sort_algorithm_tests COMMAND gui_forms_sort_algorithm_tests)

add_executable(gui_forms_sort_lab tools/sort_lab.cpp)
target_link_libraries(gui_forms_sort_lab PRIVATE gui_forms_core)

add_executable(gui_forms_image_patch_lab tools/image_patch_lab.cpp)
target_link_libraries(gui_forms_image_patch_lab PRIVATE gui_forms_core)

add_executable(gui_forms_callback_lab tools/callback_lab.cpp)
target_link_libraries(gui_forms_callback_lab PRIVATE gui_forms_core)
# The dark colour rule as a filter, for tools/relocate_colours.py.
add_executable(gui_forms_dark_colour tools/dark_colour.cpp)
target_link_libraries(gui_forms_dark_colour PRIVATE gui_forms_core)

if(TARGET gui_forms_skia)
    add_executable(gui_forms_concise_authoring_lab tools/concise_authoring_lab.cpp)
    target_link_libraries(gui_forms_concise_authoring_lab PRIVATE gui_forms_controls gui_forms_skia)
endif()
add_executable(gui_forms_owned_event_lab tools/owned_event_lab.cpp)
target_link_libraries(gui_forms_owned_event_lab PRIVATE gui_forms_core)
add_executable(gui_forms_property_capability_lab tools/property_capability_lab.cpp)
target_link_libraries(gui_forms_property_capability_lab PRIVATE gui_forms_controls)

add_executable(gui_forms_delegate_event_tests tests/delegate_event_tests.cpp)
target_link_libraries(gui_forms_delegate_event_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_delegate_event_tests COMMAND gui_forms_delegate_event_tests)

add_executable(gui_forms_live_surface_pacer_tests tests/live_surface_pacer_tests.cpp)
target_link_libraries(gui_forms_live_surface_pacer_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_live_surface_pacer_tests COMMAND gui_forms_live_surface_pacer_tests)
add_executable(gui_forms_content_surface_tests tests/content_surface_tests.cpp)
target_link_libraries(gui_forms_content_surface_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_content_surface_tests COMMAND gui_forms_content_surface_tests)
set_tests_properties(gui_forms_content_surface_tests PROPERTIES TIMEOUT 120)
add_executable(gui_forms_content_termination_tests tests/content_termination_tests.cpp)
target_link_libraries(gui_forms_content_termination_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_content_termination_tests COMMAND gui_forms_content_termination_tests)
set_tests_properties(gui_forms_content_termination_tests PROPERTIES TIMEOUT 120)
add_executable(gui_forms_content_text_editor_tests tests/content_text_editor_tests.cpp)
target_link_libraries(gui_forms_content_text_editor_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_content_text_editor_tests COMMAND gui_forms_content_text_editor_tests)
set_tests_properties(gui_forms_content_text_editor_tests PROPERTIES TIMEOUT 120)
# Hosted content: the protocol and the process cases, with the child
# program they start.
add_executable(gui_forms_hosted_content_tests tests/hosted_content_tests.cpp)
target_link_libraries(gui_forms_hosted_content_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_executable(gui_forms_hosted_child tests/hosted_child.cpp)
target_link_libraries(gui_forms_hosted_child PRIVATE gui_forms_hosted_client)
add_dependencies(gui_forms_hosted_content_tests gui_forms_hosted_child)
target_compile_definitions(gui_forms_hosted_content_tests PRIVATE
    "GUI_FORMS_HOSTED_CHILD=\"$<TARGET_FILE:gui_forms_hosted_child>\"")
add_test(NAME gui_forms_hosted_content_tests COMMAND gui_forms_hosted_content_tests)
set_tests_properties(gui_forms_hosted_content_tests PROPERTIES TIMEOUT 120)
# The content boundary's examples (examples/content_boundary,
# docs/user/content-surfaces.md): a renderer that freezes and a hosted
# program that crashes. Each runs headless as a test.
add_executable(gui_forms_example_freezing_renderer ${GUI_FORMS_SOURCE_DIR}/examples/content_boundary/freezing_renderer.cpp)
target_link_libraries(gui_forms_example_freezing_renderer PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_example_freezing_renderer COMMAND gui_forms_example_freezing_renderer)
set_tests_properties(gui_forms_example_freezing_renderer PROPERTIES TIMEOUT 60)
add_executable(gui_forms_example_crashing_program ${GUI_FORMS_SOURCE_DIR}/examples/content_boundary/crashing_program.cpp)
target_link_libraries(gui_forms_example_crashing_program PRIVATE gui_forms_hosted_client)
add_executable(gui_forms_example_hosted_crash ${GUI_FORMS_SOURCE_DIR}/examples/content_boundary/hosted_crash.cpp)
target_link_libraries(gui_forms_example_hosted_crash PRIVATE gui_forms_controls gui_forms_host_headless)
add_dependencies(gui_forms_example_hosted_crash gui_forms_example_crashing_program)
target_compile_definitions(gui_forms_example_hosted_crash PRIVATE
    "GUI_FORMS_CRASHING_PROGRAM=\"$<TARGET_FILE:gui_forms_example_crashing_program>\"")
add_test(NAME gui_forms_example_hosted_crash COMMAND gui_forms_example_hosted_crash)
set_tests_properties(gui_forms_example_hosted_crash PROPERTIES TIMEOUT 60)
add_executable(gui_forms_sub_surface_tests tests/sub_surface_tests.cpp)
target_link_libraries(gui_forms_sub_surface_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_sub_surface_tests COMMAND gui_forms_sub_surface_tests)
set_tests_properties(gui_forms_sub_surface_tests PROPERTIES TIMEOUT 120)
add_executable(gui_forms_data_grid_view_cells_tests tests/data_grid_view_cells_tests.cpp)
target_link_libraries(gui_forms_data_grid_view_cells_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_data_grid_view_cells_tests COMMAND gui_forms_data_grid_view_cells_tests)
set_tests_properties(gui_forms_data_grid_view_cells_tests PROPERTIES TIMEOUT 60)
add_executable(gui_forms_live_surface_idle_wake_tests tests/live_surface_idle_wake_tests.cpp)
target_link_libraries(gui_forms_live_surface_idle_wake_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_live_surface_idle_wake_tests COMMAND gui_forms_live_surface_idle_wake_tests)
# A hang detector, not a speed budget: the test takes well under a second
# alone and must not fail because a loaded machine schedules its threads late.
set_tests_properties(gui_forms_live_surface_idle_wake_tests PROPERTIES TIMEOUT 120)

add_executable(gui_forms_threading_tests tests/threading_tests.cpp)
target_link_libraries(gui_forms_threading_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_threading_tests COMMAND gui_forms_threading_tests)
set_tests_properties(gui_forms_threading_tests PROPERTIES TIMEOUT 15)

add_executable(gui_forms_owned_event_tests tests/owned_event_tests.cpp)
target_link_libraries(gui_forms_owned_event_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_owned_event_tests COMMAND gui_forms_owned_event_tests)

# Every example in the API reference, generated from docs/library/docstrings/
# by tools/generate_library_docs.py, compiled and run without a host.
add_executable(gui_forms_doc_examples_tests tests/doc_examples_tests.cpp)
target_link_libraries(gui_forms_doc_examples_tests PRIVATE gui_forms_controls)
# The examples are the reference's snippets as written (gui_forms
# tools/generate_library_docs.py): a snippet names a value to show it and
# leaves a record's trailing fields to their defaults, so these two warnings
# do not apply to the generated file. A source property, so it comes after the
# hardening's -Wall -Wextra on the command line.
set_source_files_properties(tests/doc_examples_tests.cpp PROPERTIES
    COMPILE_OPTIONS "-Wno-unused-variable;-Wno-missing-field-initializers")
add_test(NAME gui_forms_doc_examples_tests COMMAND gui_forms_doc_examples_tests)
# The examples marked `// compile only` need a native host: they are
# compiled against the Application library but never linked or run.
if(TARGET gui_forms_application)
    add_library(gui_forms_doc_examples_compile_only OBJECT
        tests/doc_examples_compile_only.cpp)
    target_link_libraries(gui_forms_doc_examples_compile_only PRIVATE gui_forms_application)
endif()

add_executable(gui_forms_part_composite_tests tests/part_composite_tests.cpp)
target_link_libraries(gui_forms_part_composite_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_part_composite_tests COMMAND gui_forms_part_composite_tests)
add_executable(gui_forms_paint_group_tests tests/paint_group_tests.cpp)
target_link_libraries(gui_forms_paint_group_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_paint_group_tests COMMAND gui_forms_paint_group_tests)
add_executable(gui_forms_group_effect_tests tests/group_effect_tests.cpp)
target_link_libraries(gui_forms_group_effect_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_group_effect_tests COMMAND gui_forms_group_effect_tests)
add_executable(gui_forms_text_composite_tests tests/text_composite_tests.cpp)
target_link_libraries(gui_forms_text_composite_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_text_composite_tests COMMAND gui_forms_text_composite_tests)
add_executable(gui_forms_live_surface_tests tests/live_surface_tests.cpp)
target_link_libraries(gui_forms_live_surface_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_live_surface_tests COMMAND gui_forms_live_surface_tests)
add_executable(gui_forms_opaque_live_surface_tests tests/opaque_live_surface_tests.cpp
    ${GUI_FORMS_SOURCE_DIR}/src/host/macos/application/live_surface_damage.cpp)
target_link_libraries(gui_forms_opaque_live_surface_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_opaque_live_surface_tests COMMAND gui_forms_opaque_live_surface_tests)

add_executable(gui_forms_text_store_tests tests/text_store_tests.cpp)
target_link_libraries(gui_forms_text_store_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_text_store_tests COMMAND gui_forms_text_store_tests)
# The markdown framework: parsing, source ranges, the cross-check against
# stx_rtf2md's normative reader over its dialect corpus, and RTF round trips.
add_executable(gui_forms_markdown_tests tests/markdown_tests.cpp)
target_link_libraries(gui_forms_markdown_tests PRIVATE gui_forms_core stx_rtf2md::stx_rtf2md)
# The corpus is stx's (stx_rtf2md/tests/corpus), installed with the stx package.
add_test(NAME gui_forms_markdown_tests
    COMMAND gui_forms_markdown_tests ${GUI_FORMS_STX_BUILD_PREFIX_DIR}/share/stx_text/rtf2md_corpus)
gui_forms_add_fuzz_test(markdown SOURCE ${GUI_FORMS_SOURCE_DIR}/fuzz/markdown_fuzz.cpp LIBRARIES gui_forms_core)
gui_forms_add_fuzz_test(rtf_import SOURCE ${GUI_FORMS_SOURCE_DIR}/fuzz/rtf_import_fuzz.cpp
    LIBRARIES gui_forms_core stx_rtf2md::stx_rtf2md
    ARGUMENTS ${GUI_FORMS_STX_BUILD_PREFIX_DIR}/share/stx_text/rtf2md_corpus)
add_executable(gui_forms_rtf_conversion_failure_tests tests/rtf_conversion_failure_tests.cpp)
target_link_libraries(gui_forms_rtf_conversion_failure_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_rtf_conversion_failure_tests COMMAND gui_forms_rtf_conversion_failure_tests)

add_executable(gui_forms_bounded_grapheme_tests tests/bounded_grapheme_tests.cpp)
target_include_directories(gui_forms_bounded_grapheme_tests PRIVATE ${GUI_FORMS_SOURCE_DIR}/src/core/text/unicode)
target_link_libraries(gui_forms_bounded_grapheme_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_bounded_grapheme_tests COMMAND gui_forms_bounded_grapheme_tests)

add_executable(gui_forms_document_view_tests tests/document_view_tests.cpp)
target_link_libraries(gui_forms_document_view_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_document_view_tests COMMAND gui_forms_document_view_tests)

add_executable(gui_forms_help_system_core_tests tests/help_system_core_tests.cpp)
target_link_libraries(gui_forms_help_system_core_tests PRIVATE gui_forms_controls gui_forms_core)
add_test(NAME gui_forms_help_system_core_tests COMMAND gui_forms_help_system_core_tests)
add_executable(gui_forms_locale_file_tests tests/locale_file_tests.cpp)
target_link_libraries(gui_forms_locale_file_tests PRIVATE gui_forms_controls gui_forms_core)
add_test(NAME gui_forms_locale_file_tests COMMAND gui_forms_locale_file_tests)
gui_forms_add_fuzz_test(helpbook_section SOURCE ${GUI_FORMS_SOURCE_DIR}/fuzz/helpbook_section_fuzz.cpp LIBRARIES gui_forms_core)

add_executable(gui_forms_xml_tests tests/xml_tests.cpp)
target_link_libraries(gui_forms_xml_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_xml_tests COMMAND gui_forms_xml_tests)
add_executable(gui_forms_json_reader_tests tests/json_reader_tests.cpp)
target_link_libraries(gui_forms_json_reader_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_json_reader_tests COMMAND gui_forms_json_reader_tests)
gui_forms_add_fuzz_test(json_reader SOURCE ${GUI_FORMS_SOURCE_DIR}/fuzz/json_reader_fuzz.cpp LIBRARIES gui_forms_core)

# The theme language (stage T3): numbers, tokens, diagnostics, the syntax
# tree, selectors, values, the compiler, and the deterministic mutation
# run over the tokenizer, parser and value reader.
add_executable(gui_forms_theme_number_tests tests/theme_language/theme_number_tests.cpp)
target_link_libraries(gui_forms_theme_number_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_theme_number_tests COMMAND gui_forms_theme_number_tests)

add_executable(gui_forms_theme_tokenizer_tests tests/theme_language/theme_tokenizer_tests.cpp)
target_link_libraries(gui_forms_theme_tokenizer_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_theme_tokenizer_tests COMMAND gui_forms_theme_tokenizer_tests)

add_executable(gui_forms_theme_syntax_tests tests/theme_language/theme_syntax_tests.cpp)
target_link_libraries(gui_forms_theme_syntax_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_theme_syntax_tests COMMAND gui_forms_theme_syntax_tests)
add_executable(gui_forms_form_syntax_tests tests/form_language/form_syntax_tests.cpp)
target_link_libraries(gui_forms_form_syntax_tests PRIVATE gui_forms_core)
target_compile_definitions(gui_forms_form_syntax_tests PRIVATE
    GUI_FORMS_FORM_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/form_language/corpus")
add_test(NAME gui_forms_form_syntax_tests COMMAND gui_forms_form_syntax_tests)
add_executable(gui_forms_form_model_tests tests/form_language/form_model_tests.cpp)
target_link_libraries(gui_forms_form_model_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_form_model_tests COMMAND gui_forms_form_model_tests)
add_executable(gui_forms_control_expression_tests tests/control_language/control_expression_tests.cpp)
target_link_libraries(gui_forms_control_expression_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_control_expression_tests COMMAND gui_forms_control_expression_tests)
foreach(control_test control_model_tests control_lock_tests composite_control_tests control_generator_tests
        composite_form_tests)
    add_executable(gui_forms_${control_test} tests/control_language/${control_test}.cpp)
    target_link_libraries(gui_forms_${control_test} PRIVATE gui_forms_controls)
    target_compile_definitions(gui_forms_${control_test} PRIVATE
        GUI_FORMS_COMPOSITES_DIR="${GUI_FORMS_SOURCE_DIR}/demo/composites")
    add_test(NAME gui_forms_${control_test} COMMAND gui_forms_${control_test})
endforeach()
add_executable(gui_forms_form_loader_tests tests/form_language/form_loader_tests.cpp)
target_link_libraries(gui_forms_form_loader_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_form_loader_tests COMMAND gui_forms_form_loader_tests)
add_executable(gui_forms_form_editor_tests tests/form_language/form_editor_tests.cpp)
target_link_libraries(gui_forms_form_editor_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_form_editor_tests COMMAND gui_forms_form_editor_tests)
add_executable(gui_forms_form_generator_tests tests/form_language/form_generator_tests.cpp
    ${GUI_FORMS_SOURCE_DIR}/tools/form_compiler/form_catalog.cpp ${GUI_FORMS_SOURCE_DIR}/tools/form_compiler/form_lab_catalog.cpp)
target_link_libraries(gui_forms_form_generator_tests PRIVATE gui_forms_lab gui_forms_demo_controls)
target_compile_definitions(gui_forms_form_generator_tests PRIVATE
    GUI_FORMS_FORM_GENERATED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/form_language/generated")
add_test(NAME gui_forms_form_generator_tests COMMAND gui_forms_form_generator_tests)
if(TARGET gui_forms_form_compiler)
    # The program end to end: the golden header is current, and a
    # document it cannot spell is refused with a located E812.
    add_test(NAME gui_forms_form_compiler_tests
        COMMAND gui_forms_form_compiler
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/form_language/generated/sample_dialog.form
            --class SampleDialogForm --namespace form_generated --check
            --output ${CMAKE_CURRENT_SOURCE_DIR}/tests/form_language/generated/sample_dialog_form.hpp)
    add_test(NAME gui_forms_form_compiler_refusal_tests
        COMMAND gui_forms_form_compiler
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/form_language/generated/refused_dialog.form
            --class RefusedForm --check --output refused_dialog_form.hpp)
    set_tests_properties(gui_forms_form_compiler_refusal_tests PROPERTIES
        PASS_REGULAR_EXPRESSION "E812 form_generation_refused: refused-ok")
    # The control mode refuses what the run time would: the stepper
    # waits for converters (E999).
    add_test(NAME gui_forms_control_compiler_refusal_tests
        COMMAND gui_forms_form_compiler
            ${GUI_FORMS_SOURCE_DIR}/demo/composites/stepper-field.control
            --control --class StepperField --check --output stepper_field_control.hpp)
    set_tests_properties(gui_forms_control_compiler_refusal_tests PROPERTIES
        PASS_REGULAR_EXPRESSION "E999")
    # The schema mode (cp34): the registry and two composites, written
    # for a designer; the content is checked by gui_forms_composite_form_tests.
    add_test(NAME gui_forms_form_compiler_schema_tests
        COMMAND gui_forms_form_compiler --schema
            --output ${CMAKE_CURRENT_BINARY_DIR}/form_compiler_schema_test.json
            --composite "${GUI_FORMS_SOURCE_DIR}/demo/composites/toggle-pill.control,app::TogglePill,toggle_pill_control.hpp"
            --composite "${GUI_FORMS_SOURCE_DIR}/demo/composites/card-with-action.control,app::CardWithAction,card_with_action_control.hpp")
    if(GUI_FORMS_BUILD_GENERATED_FORM_TESTS)
        # gui_forms_add_control end to end (cp34): composite controls
        # generated at build time from the worked examples.
        add_executable(gui_forms_composite_generated_tests tests/control_language/composite_generated_tests.cpp)
        target_link_libraries(gui_forms_composite_generated_tests PRIVATE gui_forms_controls)
        # Their own output directory: the gallery model generates the same
        # two documents into gui_forms_controls/.
        gui_forms_add_control(gui_forms_composite_generated_tests ${GUI_FORMS_SOURCE_DIR}/demo/composites/toggle-pill.control
            CLASS TogglePill NAMESPACE composite_generated COMPILER gui_forms_form_compiler
            OUTPUT composite_generated_controls/toggle_pill_control.hpp)
        gui_forms_add_control(gui_forms_composite_generated_tests ${GUI_FORMS_SOURCE_DIR}/demo/composites/card-with-action.control
            CLASS CardWithAction NAMESPACE composite_generated COMPILER gui_forms_form_compiler
            OUTPUT composite_generated_controls/card_with_action_control.hpp)
        # A form naming both (gui_forms_add_form COMPOSITES): the card's
        # slot takes the form's children.
        gui_forms_add_form(gui_forms_composite_generated_tests tests/control_language/composite_panel.form
            CLASS CompositePanel NAMESPACE composite_generated DERIVED COMPILER gui_forms_form_compiler
            COMPOSITES
                "${GUI_FORMS_SOURCE_DIR}/demo/composites/toggle-pill.control,composite_generated::TogglePill,toggle_pill_control.hpp"
                "${GUI_FORMS_SOURCE_DIR}/demo/composites/card-with-action.control,composite_generated::CardWithAction,card_with_action_control.hpp")
        add_test(NAME gui_forms_composite_generated_tests COMMAND gui_forms_composite_generated_tests)
        # gui_forms_add_form end to end: a header generated at build time,
        # naming an application type.
        add_executable(gui_forms_form_application_type_tests
            tests/form_language/form_application_type_tests.cpp ${GUI_FORMS_SOURCE_DIR}/tools/form_compiler/form_catalog.cpp
            ${GUI_FORMS_SOURCE_DIR}/tools/form_compiler/form_core_catalog.cpp)
        target_link_libraries(gui_forms_form_application_type_tests PRIVATE gui_forms_controls)
        target_include_directories(gui_forms_form_application_type_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/form_language)
        gui_forms_add_form(gui_forms_form_application_type_tests tests/form_language/generated/note_dialog.form
            CLASS NoteDialogForm NAMESPACE form_generated
            TYPES "note_card,form_generated::NoteCard,user_control,generated/note_card.hpp")
        add_test(NAME gui_forms_form_application_type_tests COMMAND gui_forms_form_application_type_tests)
        # gui_forms_add_form on a target sharing a source with the form
        # compiler and another target configures and builds without a
        # dependency cycle, and a changed document rebuilds it (cp33;
        # tests/cmake/form_shared_source_test.cmake).
        add_test(NAME gui_forms_form_shared_source
                 COMMAND ${CMAKE_COMMAND}
                         -DWORK_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/form_shared_source_test
                         -DSOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}
                         -DGUI_FORMS_SOURCE_DIRECTORY=${GUI_FORMS_SOURCE_DIR}
                         -DGENERATOR=${CMAKE_GENERATOR}
                         -DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
                         -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
                         "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}"
                         "-DLINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
                         -DOSX_SYSROOT=${CMAKE_OSX_SYSROOT}
                         "-DOSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}"
                         -DEXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}
                         "-DTOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
                         "-DLLVM_RUNTIME=$ENV{GUI_FORMS_LLVM_RUNTIME}"
                         "-DHARNESS_RUNTIME=$ENV{HARNESS_LLVM_RUNTIME}"
                         -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/form_shared_source_test.cmake)
        # Stage B: the classes deriving from the root's type.
        add_executable(gui_forms_form_derived_class_tests tests/form_language/form_derived_class_tests.cpp)
        target_link_libraries(gui_forms_form_derived_class_tests PRIVATE gui_forms_controls)
        target_include_directories(gui_forms_form_derived_class_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/form_language)
        gui_forms_add_form(gui_forms_form_derived_class_tests tests/form_language/generated/sample_dialog.form
            CLASS SampleDialogWindow NAMESPACE form_generated OUTPUT gui_forms_forms/sample_dialog_window.hpp
            MODEL form_generated::SampleModel MODEL_HEADER generated/sample_model.hpp)
        gui_forms_add_form(gui_forms_form_derived_class_tests tests/form_language/generated/derived_panel.form
            CLASS DerivedPanel NAMESPACE form_generated
            MODEL form_generated::SampleModel MODEL_HEADER generated/sample_model.hpp)
        gui_forms_add_form(gui_forms_form_derived_class_tests tests/form_language/generated/plain_panel.form
            CLASS PlainPanel NAMESPACE form_generated DERIVED)
        # A final root type (a layout panel), composed instead of derived.
        gui_forms_add_form(gui_forms_form_derived_class_tests tests/form_language/generated/flex_root_panel.form
            CLASS FlexRootPanel NAMESPACE form_generated
            MODEL form_generated::SampleModel MODEL_HEADER generated/sample_model.hpp)
        add_test(NAME gui_forms_form_derived_class_tests COMMAND gui_forms_form_derived_class_tests)
    endif()
endif()

add_executable(gui_forms_theme_selector_tests tests/theme_language/theme_selector_tests.cpp)
target_link_libraries(gui_forms_theme_selector_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_theme_selector_tests COMMAND gui_forms_theme_selector_tests)
add_executable(gui_forms_theme_value_tests tests/theme_language/theme_value_tests.cpp)
target_link_libraries(gui_forms_theme_value_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_theme_value_tests COMMAND gui_forms_theme_value_tests)

add_executable(gui_forms_theme_compiler_tests tests/theme_language/theme_compiler_tests.cpp)
target_link_libraries(gui_forms_theme_compiler_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_compiler_tests COMMAND gui_forms_theme_compiler_tests)
add_executable(gui_forms_theme_conformance_tests tests/theme_language/theme_conformance_tests.cpp)
target_link_libraries(gui_forms_theme_conformance_tests PRIVATE gui_forms_controls)
target_compile_definitions(gui_forms_theme_conformance_tests PRIVATE
    GUI_FORMS_THEME_CONFORMANCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/theme_language/conformance")
add_test(NAME gui_forms_theme_conformance_tests COMMAND gui_forms_theme_conformance_tests)
add_executable(gui_forms_theme_hostile_tests tests/theme_language/theme_hostile_tests.cpp)
target_link_libraries(gui_forms_theme_hostile_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_hostile_tests COMMAND gui_forms_theme_hostile_tests)
add_executable(gui_forms_theme_material_css_tests tests/theme_language/theme_material_css_tests.cpp)
target_link_libraries(gui_forms_theme_material_css_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_material_css_tests COMMAND gui_forms_theme_material_css_tests)
add_executable(gui_forms_theme_motion_css_tests tests/theme_language/theme_motion_css_tests.cpp)
target_link_libraries(gui_forms_theme_motion_css_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_motion_css_tests COMMAND gui_forms_theme_motion_css_tests)
add_executable(gui_forms_theme_animation_css_tests tests/theme_language/theme_animation_css_tests.cpp)
target_link_libraries(gui_forms_theme_animation_css_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_animation_css_tests COMMAND gui_forms_theme_animation_css_tests)
add_executable(gui_forms_theme_media_css_tests tests/theme_language/theme_media_css_tests.cpp)
target_link_libraries(gui_forms_theme_media_css_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_media_css_tests COMMAND gui_forms_theme_media_css_tests)

add_executable(gui_forms_theme_important_css_tests tests/theme_language/theme_important_css_tests.cpp)
target_link_libraries(gui_forms_theme_important_css_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_important_css_tests COMMAND gui_forms_theme_important_css_tests)
add_executable(gui_forms_theme_custom_property_css_tests tests/theme_language/theme_custom_property_css_tests.cpp)
target_link_libraries(gui_forms_theme_custom_property_css_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_custom_property_css_tests COMMAND gui_forms_theme_custom_property_css_tests)

add_executable(gui_forms_theme_inline_style_tests tests/theme_language/theme_inline_style_tests.cpp)
target_link_libraries(gui_forms_theme_inline_style_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_inline_style_tests COMMAND gui_forms_theme_inline_style_tests)
add_executable(gui_forms_theme_inline_custom_property_tests
               tests/theme_language/theme_inline_custom_property_tests.cpp)
target_link_libraries(gui_forms_theme_inline_custom_property_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_inline_custom_property_tests COMMAND gui_forms_theme_inline_custom_property_tests)
add_executable(gui_forms_theme_colour_role_css_tests tests/theme_language/theme_colour_role_css_tests.cpp)
target_link_libraries(gui_forms_theme_colour_role_css_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_colour_role_css_tests COMMAND gui_forms_theme_colour_role_css_tests)
# Early theme validation (owner request 2026-11-09): the public
# validate_theme_packages, and the build-time check run on a valid and
# a refused package.
add_executable(gui_forms_theme_validation_tests tests/theme_language/theme_validation_tests.cpp)
target_link_libraries(gui_forms_theme_validation_tests PRIVATE gui_forms_controls)
gui_forms_add_theme(gui_forms_theme_validation_tests tests/theme_language/theme_check/valid
    FUNCTION valid_theme_package NAMESPACE gui_forms_tests)
gui_forms_add_theme(gui_forms_theme_validation_tests tests/theme_language/theme_check/refused
    FUNCTION refused_theme_package NAMESPACE gui_forms_tests)
add_test(NAME gui_forms_theme_validation_tests COMMAND gui_forms_theme_validation_tests)
if(TARGET gui_forms_theme_check)
    add_test(NAME gui_forms_theme_check_accepts
        COMMAND gui_forms_theme_check "${CMAKE_CURRENT_SOURCE_DIR}/tests/theme_language/theme_check/valid")
    add_test(NAME gui_forms_theme_check_refuses
        COMMAND gui_forms_theme_check "${CMAKE_CURRENT_SOURCE_DIR}/tests/theme_language/theme_check/refused")
    set_tests_properties(gui_forms_theme_check_refuses PROPERTIES WILL_FAIL TRUE)
    add_test(NAME gui_forms_theme_check_gallery
        COMMAND gui_forms_theme_check "${GUI_FORMS_SOURCE_DIR}/demo/themes/gallery")
endif()
add_executable(gui_forms_theme_variant_tests tests/theme_variant_tests.cpp)
target_link_libraries(gui_forms_theme_variant_tests PRIVATE gui_forms_lab gui_forms_demo_controls)
add_test(NAME gui_forms_theme_variant_tests COMMAND gui_forms_theme_variant_tests)
add_executable(gui_forms_theme_sheet_package_tests tests/theme_sheet_package_tests.cpp)
target_link_libraries(gui_forms_theme_sheet_package_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_sheet_package_tests COMMAND gui_forms_theme_sheet_package_tests)
add_executable(gui_forms_theme_font_tests tests/theme_font_tests.cpp)
target_link_libraries(gui_forms_theme_font_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_font_tests COMMAND gui_forms_theme_font_tests)
add_executable(gui_forms_theme_rule_faces_tests tests/theme_rule_faces_tests.cpp)
target_link_libraries(gui_forms_theme_rule_faces_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_rule_faces_tests COMMAND gui_forms_theme_rule_faces_tests)
add_executable(gui_forms_theme_escape_tests tests/theme_language/theme_escape_tests.cpp)
target_link_libraries(gui_forms_theme_escape_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_escape_tests COMMAND gui_forms_theme_escape_tests)
add_executable(gui_forms_theme_em_font_css_tests tests/theme_language/theme_em_font_css_tests.cpp)
target_link_libraries(gui_forms_theme_em_font_css_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_em_font_css_tests COMMAND gui_forms_theme_em_font_css_tests)
add_executable(gui_forms_theme_manifest_tests tests/theme_language/theme_manifest_tests.cpp)
target_link_libraries(gui_forms_theme_manifest_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_theme_manifest_tests COMMAND gui_forms_theme_manifest_tests)
add_executable(gui_forms_theme_package_tests tests/theme_language/theme_package_tests.cpp)
target_link_libraries(gui_forms_theme_package_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_theme_package_tests COMMAND gui_forms_theme_package_tests)
add_executable(gui_forms_theme_package_compile_tests tests/theme_language/theme_package_compile_tests.cpp)
target_link_libraries(gui_forms_theme_package_compile_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_package_compile_tests COMMAND gui_forms_theme_package_compile_tests)
# The GOST paint palette package (skeudoindustrial surfaces, phase 4).
add_executable(gui_forms_theme_gost_palette_tests tests/theme_language/theme_gost_palette_tests.cpp)
target_link_libraries(gui_forms_theme_gost_palette_tests PRIVATE gui_forms_controls)
target_compile_definitions(gui_forms_theme_gost_palette_tests PRIVATE
    GUI_FORMS_GOST_PALETTE_DIR="${GUI_FORMS_SOURCE_DIR}/demo/themes/gost_paints"
    GUI_FORMS_INDUSTRIAL_THEME_DIR="${GUI_FORMS_SOURCE_DIR}/demo/themes/industrial")
add_test(NAME gui_forms_theme_gost_palette_tests COMMAND gui_forms_theme_gost_palette_tests)
add_executable(gui_forms_theme_user_theme_tests tests/theme_language/theme_user_theme_tests.cpp)
target_link_libraries(gui_forms_theme_user_theme_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_user_theme_tests COMMAND gui_forms_theme_user_theme_tests)

add_executable(gui_forms_theme_user_folder_tests tests/theme_language/theme_user_folder_tests.cpp)
target_link_libraries(gui_forms_theme_user_folder_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_user_folder_tests COMMAND gui_forms_theme_user_folder_tests)

# The theme sheet's tokenizer and parser, then the compiler, over the
# theme sheet corpus; the compiler's run must reach the palette bound
# (N113) with the corpus's palette-enumeration-bound.css.
gui_forms_add_fuzz_test(theme_sheet SOURCE ${GUI_FORMS_SOURCE_DIR}/fuzz/theme_sheet_fuzz.cpp LIBRARIES gui_forms_core
    ARGUMENTS ${GUI_FORMS_SOURCE_DIR}/fuzz/corpus/theme_sheet)
gui_forms_add_fuzz_test(theme_compile SOURCE ${GUI_FORMS_SOURCE_DIR}/fuzz/theme_compile_fuzz.cpp LIBRARIES gui_forms_controls
    ARGUMENTS ${GUI_FORMS_SOURCE_DIR}/fuzz/corpus/theme_sheet)
# The theme manifest, form documents, saved dock layouts and the
# clipboard image decoders (audit N115, I3).
gui_forms_add_fuzz_test(theme_manifest SOURCE ${GUI_FORMS_SOURCE_DIR}/fuzz/theme_manifest_fuzz.cpp LIBRARIES gui_forms_core)
gui_forms_add_fuzz_test(form_document SOURCE ${GUI_FORMS_SOURCE_DIR}/fuzz/form_document_fuzz.cpp LIBRARIES gui_forms_controls)
gui_forms_add_fuzz_test(dock_layout SOURCE ${GUI_FORMS_SOURCE_DIR}/fuzz/dock_layout_fuzz.cpp LIBRARIES gui_forms_core)
gui_forms_add_fuzz_test(clipboard_image SOURCE ${GUI_FORMS_SOURCE_DIR}/fuzz/clipboard_image_fuzz.cpp LIBRARIES gui_forms_core)
# The theme a process is started with (--theme, GUI_FORMS_THEME_SHEET;
# gui_forms/application/user_themes): in process, then programs with no
# theme code of their own launched with each request (the C ABI's in
# cmake/CAbi.cmake). A program's expectation and policy come from its
# environment, so its command line holds only what a user would type.
add_executable(gui_forms_theme_request_tests tests/theme_language/theme_request_tests.cpp)
target_link_libraries(gui_forms_theme_request_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_request_tests COMMAND gui_forms_theme_request_tests)
set(GUI_FORMS_THEME_REQUEST_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/tests/theme_language/theme_request")
# gui_forms_theme_request_case(<test> PROGRAM <target> EXPECT <outcome>
#     [POLICY refused|replaces] [SHEET_ENV <path>] [APPLICATION] [ARGS <arguments>...])
function(gui_forms_theme_request_case test)
    cmake_parse_arguments(PARSE_ARGV 1 CASE "APPLICATION" "PROGRAM;EXPECT;POLICY;SHEET_ENV" "ARGS")
    add_test(NAME ${test} COMMAND ${CASE_PROGRAM} ${CASE_ARGS})
    set(_modification "GF_EXPECT_THEME_REQUEST=set:${CASE_EXPECT}")
    if(CASE_POLICY)
        list(APPEND _modification "GF_TEST_POLICY=set:${CASE_POLICY}")
    else()
        list(APPEND _modification "GF_TEST_POLICY=unset:")
    endif()
    if(CASE_SHEET_ENV)
        list(APPEND _modification "GUI_FORMS_THEME_SHEET=set:${CASE_SHEET_ENV}")
    else()
        list(APPEND _modification "GUI_FORMS_THEME_SHEET=unset:")
    endif()
    if(CASE_APPLICATION)
        list(APPEND _modification "GF_TEST_APPLICATION=set:1")
    else()
        list(APPEND _modification "GF_TEST_APPLICATION=unset:")
    endif()
    set_tests_properties(${test} PROPERTIES ENVIRONMENT_MODIFICATION "${_modification}")
endfunction()
# The cases every launched program runs, its tests named <prefix>_<case>.
function(gui_forms_theme_request_cases prefix program)
    set(_fixtures "${GUI_FORMS_THEME_REQUEST_FIXTURES}")
    gui_forms_theme_request_case(${prefix}_none PROGRAM ${program} EXPECT none)
    gui_forms_theme_request_case(${prefix}_sheet PROGRAM ${program} EXPECT applied
        ARGS --theme "${_fixtures}/good.css")
    gui_forms_theme_request_case(${prefix}_built_in PROGRAM ${program} EXPECT built_in ARGS --theme built-in)
    gui_forms_theme_request_case(${prefix}_name PROGRAM ${program} EXPECT applied ARGS --theme harbour)
    gui_forms_theme_request_case(${prefix}_unknown_name PROGRAM ${program} EXPECT unknown_name
        ARGS --theme dusk)
    gui_forms_theme_request_case(${prefix}_bad_sheet PROGRAM ${program} EXPECT refused
        ARGS --theme "${_fixtures}/bad.css")
    gui_forms_theme_request_case(${prefix}_missing_sheet PROGRAM ${program} EXPECT unreadable
        ARGS --theme "${_fixtures}/absent.css")
    gui_forms_theme_request_case(${prefix}_policy_refused PROGRAM ${program} EXPECT ignored_by_policy
        POLICY refused ARGS --theme "${_fixtures}/good.css")
    gui_forms_theme_request_case(${prefix}_policy_replaces PROGRAM ${program} EXPECT applied
        POLICY replaces ARGS --theme "${_fixtures}/good.css")
    gui_forms_theme_request_case(${prefix}_environment PROGRAM ${program} EXPECT applied
        SHEET_ENV "${_fixtures}/good.css")
    gui_forms_theme_request_case(${prefix}_command_line_wins PROGRAM ${program} EXPECT refused
        SHEET_ENV "${_fixtures}/good.css" ARGS --theme "${_fixtures}/bad.css")
    gui_forms_theme_request_case(${prefix}_malformed PROGRAM ${program} EXPECT malformed ARGS --theme)
endfunction()
add_executable(gui_forms_theme_request_app tests/theme_language/theme_request_app.cpp)
target_link_libraries(gui_forms_theme_request_app PRIVATE gui_forms_controls)
# The user theme `harbour`, in the `themes` folder beside the program.
add_custom_command(TARGET gui_forms_theme_request_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GUI_FORMS_THEME_REQUEST_FIXTURES}/harbour.css"
        "$<TARGET_FILE_DIR:gui_forms_theme_request_app>/themes/harbour.css")
gui_forms_theme_request_cases(gui_forms_theme_request_app gui_forms_theme_request_app)
# A user sheet naming an effect the built-in registry does not hold is
# refused (E417), as a program package naming one is.
gui_forms_theme_request_case(gui_forms_theme_request_app_misspelt_effect PROGRAM gui_forms_theme_request_app
    EXPECT refused ARGS --theme "${GUI_FORMS_THEME_REQUEST_FIXTURES}/misspelt-effect.css")

add_executable(gui_forms_dock_layout_tests tests/dock_layout_tests.cpp)
target_link_libraries(gui_forms_dock_layout_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_dock_layout_tests COMMAND gui_forms_dock_layout_tests)
add_executable(gui_forms_dock_layout_frames_tests tests/dock_layout_frames_tests.cpp)
target_link_libraries(gui_forms_dock_layout_frames_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_dock_layout_frames_tests COMMAND gui_forms_dock_layout_frames_tests)

add_executable(gui_forms_grapheme_conformance_tests
    tests/grapheme_conformance_tests.cpp
)
target_include_directories(gui_forms_grapheme_conformance_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
target_link_libraries(gui_forms_grapheme_conformance_tests PRIVATE
    gui_forms_core
)
add_test(NAME gui_forms_grapheme_conformance_tests
         COMMAND gui_forms_grapheme_conformance_tests)

add_executable(gui_forms_text_shaping_tests tests/text_shaping_tests.cpp)
target_link_libraries(gui_forms_text_shaping_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_text_shaping_tests COMMAND gui_forms_text_shaping_tests)

if(TARGET gui_forms_text_engine)
    add_executable(gui_forms_glyph_run_raster_tests tests/glyph_run_raster_tests.cpp)
    target_link_libraries(gui_forms_glyph_run_raster_tests PRIVATE gui_forms_text_engine)
    target_compile_definitions(gui_forms_glyph_run_raster_tests PRIVATE
        GUI_FORMS_TEST_RAPIDS_REGULAR="${GUI_FORMS_SOURCE_DIR}/assets/fonts/PortsmouthRapids.ttf"
        GUI_FORMS_TEST_CARLITO_REGULAR="${GUI_FORMS_SOURCE_DIR}/assets/fonts/Carlito-Regular.ttf")
    add_test(NAME gui_forms_glyph_run_raster_tests COMMAND gui_forms_glyph_run_raster_tests)
    add_executable(gui_forms_bounded_shape_tests tests/bounded_shape_tests.cpp)
    target_link_libraries(gui_forms_bounded_shape_tests PRIVATE gui_forms_text_engine)
    add_test(NAME gui_forms_bounded_shape_tests COMMAND gui_forms_bounded_shape_tests "${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    add_executable(gui_forms_font_metrics_differential_tests tests/font_metrics_differential_tests.cpp)
    target_link_libraries(gui_forms_font_metrics_differential_tests PRIVATE gui_forms_font_metrics_differential)
    add_test(NAME gui_forms_font_metrics_differential_tests COMMAND gui_forms_font_metrics_differential_tests
        "${GUI_FORMS_SOURCE_DIR}/assets/fonts"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/font_metrics_differential/corpus/forms_latin_v1.txt")
    # Stage F's metric gate (step F4): bundled candidates against the
    # committed reference snapshots, on every host.
    add_executable(gui_forms_font_metrics_gate_tests tests/font_metrics_gate_tests.cpp)
    target_link_libraries(gui_forms_font_metrics_gate_tests PRIVATE
        gui_forms_font_metrics_differential gui_drawing_core)
    add_test(NAME gui_forms_font_metrics_gate_tests COMMAND gui_forms_font_metrics_gate_tests
        "${GUI_FORMS_SOURCE_DIR}/assets/fonts"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/font_metrics_differential/corpus/forms_latin_v1.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/font_metrics_differential/reference_snapshots")

    add_executable(gui_forms_bounded_shape_workspace_tests tests/bounded_shape_workspace_tests.cpp)
    target_link_libraries(gui_forms_bounded_shape_workspace_tests PRIVATE gui_forms_text_engine)
    add_test(NAME gui_forms_bounded_shape_workspace_tests COMMAND gui_forms_bounded_shape_workspace_tests "${GUI_FORMS_SOURCE_DIR}/assets/fonts")

    add_executable(gui_forms_prepared_window_shape_bench tests/prepared_window_shape_bench.cpp)
    target_link_libraries(gui_forms_prepared_window_shape_bench PRIVATE gui_forms_text_engine)
    add_test(NAME gui_forms_prepared_window_shape_preview_verify
        COMMAND gui_forms_prepared_window_shape_bench --fonts "${GUI_FORMS_SOURCE_DIR}/assets/fonts" --scene preview-seven --verify-only)
    add_test(NAME gui_forms_prepared_window_shape_rows_verify
        COMMAND gui_forms_prepared_window_shape_bench --fonts "${GUI_FORMS_SOURCE_DIR}/assets/fonts" --scene window-512 --verify-only)

    # The bundled fallback faces' compiled coverage tables against the faces' own character maps (STXlib).
    add_executable(gui_forms_bundled_font_coverage_tests tests/bundled_font_coverage_tests.cpp)
    target_link_libraries(gui_forms_bundled_font_coverage_tests PRIVATE gui_forms_core stxlib::algorithms)
    target_compile_definitions(gui_forms_bundled_font_coverage_tests PRIVATE
        GUI_FORMS_TEST_FONT_DIRECTORY="${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    add_test(NAME gui_forms_bundled_font_coverage_tests COMMAND gui_forms_bundled_font_coverage_tests)
    add_executable(gui_forms_harfbuzz_font_engine_tests
        tests/harfbuzz_font_engine_tests.cpp
    )
    target_compile_definitions(gui_forms_harfbuzz_font_engine_tests PRIVATE
        GUI_FORMS_TEST_RAPIDS_REGULAR="${GUI_FORMS_SOURCE_DIR}/assets/fonts/PortsmouthRapids.ttf"
        GUI_FORMS_TEST_RAPIDS_BOLD="${GUI_FORMS_SOURCE_DIR}/assets/fonts/PortsmouthRapids-Bold.ttf"
        GUI_FORMS_TEST_CARLITO_REGULAR="${GUI_FORMS_SOURCE_DIR}/assets/fonts/Carlito-Regular.ttf"
        GUI_FORMS_TEST_CARLITO_BOLD="${GUI_FORMS_SOURCE_DIR}/assets/fonts/Carlito-Bold.ttf"
        GUI_FORMS_TEST_COUSINE_REGULAR="${GUI_FORMS_SOURCE_DIR}/assets/fonts/Cousine-Regular.ttf"
        GUI_FORMS_TEST_NOTO_ARABIC_REGULAR="${GUI_FORMS_SOURCE_DIR}/assets/fonts/NotoSansArabic-Minimal.ttf"
        GUI_FORMS_TEST_NOTO_HEBREW_REGULAR="${GUI_FORMS_SOURCE_DIR}/assets/fonts/NotoSansHebrew-Regular.ttf"
        GUI_FORMS_TEST_NOTO_CJK_REGULAR="${GUI_FORMS_SOURCE_DIR}/assets/fonts/NotoSansCJKjp-Subset.otf"
        GUI_FORMS_TEST_NOTO_CJK_KOREAN="${GUI_FORMS_SOURCE_DIR}/assets/fonts/NotoSansCJKkr-Subset.otf"
        GUI_FORMS_TEST_NOTO_EMOJI="${GUI_FORMS_SOURCE_DIR}/assets/fonts/NotoEmoji-Regular.ttf"
        GUI_FORMS_TEST_SELAWIK_REGULAR="${GUI_FORMS_SOURCE_DIR}/assets/fonts/Selawik-Regular.ttf"
        GUI_FORMS_TEST_SELAWIK_BOLD="${GUI_FORMS_SOURCE_DIR}/assets/fonts/Selawik-Bold.ttf"
    )
    target_link_libraries(gui_forms_harfbuzz_font_engine_tests PRIVATE
        gui_forms_text_engine)
    add_test(NAME gui_forms_harfbuzz_font_engine_tests
             COMMAND gui_forms_harfbuzz_font_engine_tests)
    # Application font families: validation, loading, composite looks, refusal in stock
    # controls, quarantine and unload while in use.
    add_executable(gui_forms_app_font_tests tests/app_font_tests.cpp)
    target_compile_definitions(gui_forms_app_font_tests PRIVATE
        GUI_FORMS_TEST_CARLITO_REGULAR="${GUI_FORMS_SOURCE_DIR}/assets/fonts/Carlito-Regular.ttf")
    target_link_libraries(gui_forms_app_font_tests PRIVATE gui_forms_controls gui_forms_text_engine)
    add_test(NAME gui_forms_app_font_tests COMMAND gui_forms_app_font_tests)
endif()

add_executable(gui_forms_basic_controls_tests tests/basic_controls_tests.cpp)
target_link_libraries(gui_forms_basic_controls_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_basic_controls_tests COMMAND gui_forms_basic_controls_tests)

add_executable(gui_forms_connected_controls_tests
    tests/connected_controls_tests.cpp)
target_link_libraries(gui_forms_connected_controls_tests PRIVATE
    gui_forms_controls)
add_test(NAME gui_forms_connected_controls_tests
         COMMAND gui_forms_connected_controls_tests)

add_executable(gui_forms_scrollable_control_tests
    tests/scrollable_control_tests.cpp)
target_link_libraries(gui_forms_scrollable_control_tests PRIVATE
    gui_forms_controls)
add_test(NAME gui_forms_scrollable_control_tests
         COMMAND gui_forms_scrollable_control_tests)

add_executable(gui_forms_input_controls_tests tests/input_controls_tests.cpp)
target_link_libraries(gui_forms_input_controls_tests PRIVATE
    gui_forms_controls
    gui_forms_host_headless
)
add_test(NAME gui_forms_input_controls_tests COMMAND gui_forms_input_controls_tests)

add_executable(gui_forms_multiline_text_box_tests tests/multiline_text_box_tests.cpp)
target_link_libraries(gui_forms_multiline_text_box_tests PRIVATE
    gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_multiline_text_box_tests COMMAND gui_forms_multiline_text_box_tests)

add_executable(gui_forms_text_decorations_tests tests/text_decorations_tests.cpp)
target_link_libraries(gui_forms_text_decorations_tests PRIVATE
    gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_text_decorations_tests COMMAND gui_forms_text_decorations_tests)

add_executable(gui_forms_semantic_text_tests tests/semantic_text_tests.cpp)
target_link_libraries(gui_forms_semantic_text_tests PRIVATE
    gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_semantic_text_tests COMMAND gui_forms_semantic_text_tests)

add_executable(gui_forms_inspection_controls_tests
    tests/inspection_controls_tests.cpp)
target_link_libraries(gui_forms_inspection_controls_tests PRIVATE
    gui_forms_controls)
add_test(NAME gui_forms_inspection_controls_tests
         COMMAND gui_forms_inspection_controls_tests)

add_executable(gui_forms_visual_inspection_tests
    tests/visual_inspection_tests.cpp)
target_link_libraries(gui_forms_visual_inspection_tests PRIVATE
    gui_forms_controls)
add_test(NAME gui_forms_visual_inspection_tests
         COMMAND gui_forms_visual_inspection_tests)

add_executable(gui_forms_instrument_controls_tests
    tests/instrument_controls_tests.cpp)
target_link_libraries(gui_forms_instrument_controls_tests PRIVATE
    gui_forms_lab)
add_test(NAME gui_forms_instrument_controls_tests
         COMMAND gui_forms_instrument_controls_tests)

add_executable(gui_forms_collection_cell_tests tests/collection_cell_tests.cpp)
target_link_libraries(gui_forms_collection_cell_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_collection_cell_tests COMMAND gui_forms_collection_cell_tests)

add_executable(gui_forms_checked_list_box_tests
    tests/checked_list_box_tests.cpp
)
target_link_libraries(gui_forms_checked_list_box_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_checked_list_box_tests
         COMMAND gui_forms_checked_list_box_tests)

add_executable(gui_forms_collection_controls_tests
    tests/collection_controls_tests.cpp)
target_link_libraries(gui_forms_collection_controls_tests PRIVATE
    gui_forms_lab)
add_test(NAME gui_forms_collection_controls_tests
         COMMAND gui_forms_collection_controls_tests)

add_executable(gui_forms_object_details_failure_tests
    tests/object_details_failure_tests.cpp)
target_link_libraries(gui_forms_object_details_failure_tests PRIVATE
    gui_forms_controls)
add_test(NAME gui_forms_object_details_failure_tests
         COMMAND gui_forms_object_details_failure_tests)

add_executable(gui_forms_menu_controls_tests
    tests/menu_controls_tests.cpp)
target_link_libraries(gui_forms_menu_controls_tests PRIVATE
    gui_forms_controls)
add_test(NAME gui_forms_menu_controls_tests
         COMMAND gui_forms_menu_controls_tests)

add_executable(gui_forms_overlay_controls_tests
    tests/overlay_controls_tests.cpp)
target_link_libraries(gui_forms_overlay_controls_tests PRIVATE
    gui_forms_controls)
add_test(NAME gui_forms_overlay_controls_tests
         COMMAND gui_forms_overlay_controls_tests)

add_executable(gui_forms_range_controls_tests tests/range_controls_tests.cpp)
target_link_libraries(gui_forms_range_controls_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_range_controls_tests COMMAND gui_forms_range_controls_tests)

add_executable(gui_forms_domain_up_down_tests tests/domain_up_down_tests.cpp)
target_link_libraries(gui_forms_domain_up_down_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_domain_up_down_tests COMMAND gui_forms_domain_up_down_tests)
add_executable(gui_forms_editor_composite_capabilities_tests tests/editor_composite_capabilities_tests.cpp)
target_link_libraries(gui_forms_editor_composite_capabilities_tests PRIVATE gui_forms_lab)
add_test(NAME gui_forms_editor_composite_capabilities_tests COMMAND gui_forms_editor_composite_capabilities_tests)
add_executable(gui_forms_item_view_capabilities_tests tests/item_view_capabilities_tests.cpp)
target_link_libraries(gui_forms_item_view_capabilities_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_item_view_capabilities_tests COMMAND gui_forms_item_view_capabilities_tests)
add_executable(gui_forms_picker_capabilities_tests tests/picker_capabilities_tests.cpp)
target_link_libraries(gui_forms_picker_capabilities_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_picker_capabilities_tests COMMAND gui_forms_picker_capabilities_tests)
add_executable(gui_forms_component_capabilities_tests tests/component_capabilities_tests.cpp)
target_link_libraries(gui_forms_component_capabilities_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_component_capabilities_tests COMMAND gui_forms_component_capabilities_tests)
add_executable(gui_forms_display_controls_tests tests/display_controls_tests.cpp)
target_link_libraries(gui_forms_display_controls_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_display_controls_tests COMMAND gui_forms_display_controls_tests)
add_executable(gui_forms_final_controls_tests tests/final_controls_tests.cpp)
target_link_libraries(gui_forms_final_controls_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_final_controls_tests COMMAND gui_forms_final_controls_tests)
add_executable(gui_forms_auto_complete_tests tests/auto_complete_tests.cpp)
target_link_libraries(gui_forms_auto_complete_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_auto_complete_tests COMMAND gui_forms_auto_complete_tests)
# Touchpad gestures: shape, routing, latching and the headless pinch.
add_executable(gui_forms_gesture_event_tests tests/gesture_event_tests.cpp)
target_link_libraries(gui_forms_gesture_event_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_gesture_event_tests COMMAND gui_forms_gesture_event_tests)
add_executable(gui_forms_combo_box_filter_tests tests/combo_box_filter_tests.cpp)
target_link_libraries(gui_forms_combo_box_filter_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_combo_box_filter_tests COMMAND gui_forms_combo_box_filter_tests)
add_executable(gui_forms_combo_box_paint_tests tests/combo_box_paint_tests.cpp)
target_link_libraries(gui_forms_combo_box_paint_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_combo_box_paint_tests COMMAND gui_forms_combo_box_paint_tests)
add_executable(gui_forms_token_box_tests tests/token_box_tests.cpp)
target_link_libraries(gui_forms_token_box_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_token_box_tests COMMAND gui_forms_token_box_tests)
add_executable(gui_forms_text_box_escape_tests tests/text_box_escape_tests.cpp)
target_link_libraries(gui_forms_text_box_escape_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_text_box_escape_tests COMMAND gui_forms_text_box_escape_tests)
add_executable(gui_forms_text_box_enter_tests tests/text_box_enter_tests.cpp)
target_link_libraries(gui_forms_text_box_enter_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_text_box_enter_tests COMMAND gui_forms_text_box_enter_tests)

add_executable(gui_forms_tool_strip_tests tests/tool_strip_tests.cpp)
target_link_libraries(gui_forms_tool_strip_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_tool_strip_tests COMMAND gui_forms_tool_strip_tests)
add_executable(gui_forms_strip_merge_tests tests/strip_merge_tests.cpp)
target_link_libraries(gui_forms_strip_merge_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_strip_merge_tests COMMAND gui_forms_strip_merge_tests)
add_executable(gui_forms_sweep_scope_tests tests/sweep_scope_tests.cpp)
target_link_libraries(gui_forms_sweep_scope_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_sweep_scope_tests COMMAND gui_forms_sweep_scope_tests)

# The node canvas's look, theme side (cp31): anatomy, recipes in light,
# dark and high contrast, grid variants, the texture clamp, the paint
# commands and the link_flow effect.
add_executable(gui_forms_canvas_look_tests tests/canvas_look_tests.cpp)
target_link_libraries(gui_forms_canvas_look_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_canvas_look_tests COMMAND gui_forms_canvas_look_tests)
add_executable(gui_forms_plot_view_tests tests/plot_view_tests.cpp)
target_link_libraries(gui_forms_plot_view_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_plot_view_tests COMMAND gui_forms_plot_view_tests)
add_executable(gui_forms_plot3d_view_tests tests/plot3d_view_tests.cpp)
target_link_libraries(gui_forms_plot3d_view_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_plot3d_view_tests COMMAND gui_forms_plot3d_view_tests)
add_executable(gui_forms_event_payload_lifetime_tests tests/event_payload_lifetime_tests.cpp)
target_link_libraries(gui_forms_event_payload_lifetime_tests PRIVATE gui_forms_lab)
add_test(NAME gui_forms_event_payload_lifetime_tests COMMAND gui_forms_event_payload_lifetime_tests)
add_executable(gui_forms_page_strip_tests tests/page_strip_tests.cpp)
target_link_libraries(gui_forms_page_strip_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_page_strip_tests COMMAND gui_forms_page_strip_tests)
add_executable(gui_forms_page_strip_side_tests tests/page_strip_side_tests.cpp)
target_link_libraries(gui_forms_page_strip_side_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_page_strip_side_tests COMMAND gui_forms_page_strip_side_tests)
add_executable(gui_forms_page_strip_drag_tests tests/page_strip_drag_tests.cpp)
target_link_libraries(gui_forms_page_strip_drag_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_page_strip_drag_tests COMMAND gui_forms_page_strip_drag_tests)
add_executable(gui_forms_page_view_tests tests/page_view_tests.cpp)
target_link_libraries(gui_forms_page_view_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_page_view_tests COMMAND gui_forms_page_view_tests)
add_executable(gui_forms_application_instance_tests tests/application_instance_tests.cpp)
target_link_libraries(gui_forms_application_instance_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_application_instance_tests COMMAND gui_forms_application_instance_tests)
add_executable(gui_forms_mouse_events_tests tests/mouse_events_tests.cpp)
target_link_libraries(gui_forms_mouse_events_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_mouse_events_tests COMMAND gui_forms_mouse_events_tests)
add_executable(gui_forms_key_events_tests tests/key_events_tests.cpp)
target_link_libraries(gui_forms_key_events_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_key_events_tests COMMAND gui_forms_key_events_tests)
add_executable(gui_forms_mouse_event_order_tests tests/mouse_event_order_tests.cpp)
target_link_libraries(gui_forms_mouse_event_order_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_mouse_event_order_tests COMMAND gui_forms_mouse_event_order_tests)
add_executable(gui_forms_control_raiser_tests tests/control_raiser_tests.cpp)
target_link_libraries(gui_forms_control_raiser_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_control_raiser_tests COMMAND gui_forms_control_raiser_tests)
add_executable(gui_forms_key_commands_tests tests/key_commands_tests.cpp)
target_link_libraries(gui_forms_key_commands_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_key_commands_tests COMMAND gui_forms_key_commands_tests)
# The X11 host's XSETTINGS parser takes plain bytes and no Xlib, so it is
# tested on every platform.
add_executable(gui_forms_xsettings_parse_tests
    tests/xsettings_parse_tests.cpp ${GUI_FORMS_SOURCE_DIR}/src/host/linux/settings/xsettings_parse.cpp)
target_include_directories(gui_forms_xsettings_parse_tests PRIVATE ${GUI_FORMS_SOURCE_DIR}/include)
# The public headers it includes refer to the checked-borrows link
# marker in a checked build, which only the core library defines.
target_link_libraries(gui_forms_xsettings_parse_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_xsettings_parse_tests COMMAND gui_forms_xsettings_parse_tests)
# The X11 host's compositor frame synchronisation takes plain numbers and
# no Xlib, so it is tested on every platform.
add_executable(gui_forms_x11_frame_sync_tests
    tests/x11_frame_sync_tests.cpp ${GUI_FORMS_SOURCE_DIR}/src/host/linux/frame_sync/x11_frame_sync.cpp)
target_include_directories(gui_forms_x11_frame_sync_tests PRIVATE ${GUI_FORMS_SOURCE_DIR}/include)
target_link_libraries(gui_forms_x11_frame_sync_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_x11_frame_sync_tests COMMAND gui_forms_x11_frame_sync_tests)
# The hosts' locale pattern conversions (UTS #35 for macOS, strftime for
# Linux, format pictures for Windows) take plain text, so they are tested
# on every platform; each platform's own reader is compiled in where it
# builds.
add_executable(gui_forms_host_locale_tests
    tests/host_locale_tests.cpp
    ${GUI_FORMS_SOURCE_DIR}/src/host/macos/locale/icu_date_pattern.cpp
    ${GUI_FORMS_SOURCE_DIR}/src/host/linux/locale/strftime_date_pattern.cpp
    ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/locale/windows_date_pattern.cpp)
target_include_directories(gui_forms_host_locale_tests PRIVATE ${GUI_FORMS_SOURCE_DIR}/include)
target_link_libraries(gui_forms_host_locale_tests PRIVATE gui_forms_core)
if(APPLE)
    find_library(GUI_FORMS_COREFOUNDATION CoreFoundation REQUIRED)
    target_sources(gui_forms_host_locale_tests PRIVATE ${GUI_FORMS_SOURCE_DIR}/src/surface/macos/locale/macos_locale.cpp)
    target_compile_definitions(gui_forms_host_locale_tests PRIVATE GUI_FORMS_TEST_MACOS_LOCALE=1)
    target_link_libraries(gui_forms_host_locale_tests PRIVATE ${GUI_FORMS_COREFOUNDATION})
elseif(UNIX)
    target_sources(gui_forms_host_locale_tests PRIVATE ${GUI_FORMS_SOURCE_DIR}/src/surface/linux/locale/posix_locale.cpp)
    target_compile_definitions(gui_forms_host_locale_tests PRIVATE GUI_FORMS_TEST_POSIX_LOCALE=1)
elseif(WIN32)
    target_sources(gui_forms_host_locale_tests PRIVATE ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/locale/windows_locale.cpp)
    target_compile_definitions(gui_forms_host_locale_tests PRIVATE
        ${GUI_FORMS_WINDOWS_DEFINITIONS} GUI_FORMS_TEST_WINDOWS_LOCALE=1)
endif()
add_test(NAME gui_forms_host_locale_tests COMMAND gui_forms_host_locale_tests)
# The single-surface window manager's desk, input queue and compositing,
# and the browser host's input rules, present record and dialog rules:
# plain C++, tested on every platform.
add_executable(gui_forms_surface_desk_tests
    tests/surface_desk_tests.cpp
    ${GUI_FORMS_SOURCE_DIR}/src/host/surface/compose/surface_compose.cpp
    ${GUI_FORMS_SOURCE_DIR}/src/host/surface/desk/surface_desk.cpp
    ${GUI_FORMS_SOURCE_DIR}/src/host/surface/input/surface_input.cpp)
target_include_directories(gui_forms_surface_desk_tests PRIVATE ${GUI_FORMS_SOURCE_DIR}/include)
target_link_libraries(gui_forms_surface_desk_tests PRIVATE gui_forms_core Threads::Threads)
add_test(NAME gui_forms_surface_desk_tests COMMAND gui_forms_surface_desk_tests)
# The presentation shim (docs/user/presentation.md): geometry, the
# normative trace order, and both paths with fallback over in-memory
# fakes. Plain C++, tested on every platform.
add_executable(gui_forms_presentation_shim_tests tests/presentation_shim_tests.cpp)
target_include_directories(gui_forms_presentation_shim_tests PRIVATE ${GUI_FORMS_SOURCE_DIR}/include)
target_link_libraries(gui_forms_presentation_shim_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_presentation_shim_tests COMMAND gui_forms_presentation_shim_tests)
# The first-frame show gate (first_frame_show.hpp): the policy each
# host carries out before a new window goes on screen. Plain C++.
add_executable(gui_forms_first_frame_show_tests tests/first_frame_show_tests.cpp)
target_include_directories(gui_forms_first_frame_show_tests PRIVATE ${GUI_FORMS_SOURCE_DIR}/include)
target_link_libraries(gui_forms_first_frame_show_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_first_frame_show_tests COMMAND gui_forms_first_frame_show_tests)
add_executable(gui_forms_host_scroll_damage_tests tests/host_scroll_damage_tests.cpp)
target_include_directories(gui_forms_host_scroll_damage_tests PRIVATE ${GUI_FORMS_SOURCE_DIR}/include)
target_link_libraries(gui_forms_host_scroll_damage_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_host_scroll_damage_tests COMMAND gui_forms_host_scroll_damage_tests)
add_executable(gui_forms_web_host_rules_tests
    tests/web_host_rules_tests.cpp
    ${GUI_FORMS_SOURCE_DIR}/src/host/web/application/web_dialog_rules.cpp
    ${GUI_FORMS_SOURCE_DIR}/src/host/web/input/web_input_rules.cpp
    ${GUI_FORMS_SOURCE_DIR}/src/host/web/page/web_present.cpp)
target_include_directories(gui_forms_web_host_rules_tests PRIVATE ${GUI_FORMS_SOURCE_DIR}/include)
target_link_libraries(gui_forms_web_host_rules_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_web_host_rules_tests COMMAND gui_forms_web_host_rules_tests)
# The X11 host's computed click counts and context keys take plain
# values and no Xlib, so they are tested on every platform.
add_executable(gui_forms_linux_input_rules_tests
    tests/linux_input_rules_tests.cpp ${GUI_FORMS_SOURCE_DIR}/src/host/linux/application/linux_input_rules.cpp)
target_include_directories(gui_forms_linux_input_rules_tests PRIVATE ${GUI_FORMS_SOURCE_DIR}/include)
# The public headers it includes refer to the checked-borrows link
# marker in a checked build, which only the core library defines.
target_link_libraries(gui_forms_linux_input_rules_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_linux_input_rules_tests COMMAND gui_forms_linux_input_rules_tests)
# The Windows host's DirectManipulation rules take plain values and no
# windows.h, so they are tested on every platform.
add_executable(gui_forms_windows_manipulation_rules_tests
    tests/windows_manipulation_rules_tests.cpp
    ${GUI_FORMS_SOURCE_DIR}/src/host/windows/input/windows_manipulation_rules.cpp)
target_include_directories(gui_forms_windows_manipulation_rules_tests PRIVATE ${GUI_FORMS_SOURCE_DIR}/include)
target_link_libraries(gui_forms_windows_manipulation_rules_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_windows_manipulation_rules_tests COMMAND gui_forms_windows_manipulation_rules_tests)
add_executable(gui_forms_page_view_document_tests tests/page_view_document_tests.cpp)
target_link_libraries(gui_forms_page_view_document_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_page_view_document_tests COMMAND gui_forms_page_view_document_tests)
add_executable(gui_forms_navigation_pane_tests tests/navigation_pane_tests.cpp)
target_link_libraries(gui_forms_navigation_pane_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_navigation_pane_tests COMMAND gui_forms_navigation_pane_tests)
add_executable(gui_forms_dock_site_tests tests/dock_site_tests.cpp)
target_link_libraries(gui_forms_dock_site_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_dock_site_tests COMMAND gui_forms_dock_site_tests)
add_executable(gui_forms_mdi_tests tests/mdi_tests.cpp)
target_link_libraries(gui_forms_mdi_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_mdi_tests COMMAND gui_forms_mdi_tests)
add_executable(gui_forms_dock_auto_hide_tests tests/dock_auto_hide_tests.cpp)
target_link_libraries(gui_forms_dock_auto_hide_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_dock_auto_hide_tests COMMAND gui_forms_dock_auto_hide_tests)
add_executable(gui_forms_tree_list_view_tests tests/tree_list_view_tests.cpp)
target_link_libraries(gui_forms_tree_list_view_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_tree_list_view_tests COMMAND gui_forms_tree_list_view_tests)

add_executable(gui_forms_pie_chart_tests tests/pie_chart_tests.cpp)
target_link_libraries(gui_forms_pie_chart_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_pie_chart_tests COMMAND gui_forms_pie_chart_tests)
add_executable(gui_forms_button_text_box_tests tests/button_text_box_tests.cpp)
target_link_libraries(gui_forms_button_text_box_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_button_text_box_tests COMMAND gui_forms_button_text_box_tests)
add_executable(gui_forms_popup_container_box_tests tests/popup_container_box_tests.cpp)
target_link_libraries(gui_forms_popup_container_box_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_popup_container_box_tests COMMAND gui_forms_popup_container_box_tests)
add_executable(gui_forms_checked_combo_box_tests tests/checked_combo_box_tests.cpp)
target_link_libraries(gui_forms_checked_combo_box_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_checked_combo_box_tests COMMAND gui_forms_checked_combo_box_tests)
add_executable(gui_forms_number_text_tests tests/number_text_tests.cpp)
target_link_libraries(gui_forms_number_text_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_number_text_tests COMMAND gui_forms_number_text_tests)
# 77: the C-locale half passed but no comma locale is installed, so the
# half that proves the parser ignores LC_NUMERIC did not run.
set_tests_properties(gui_forms_number_text_tests PROPERTIES SKIP_RETURN_CODE 77)

add_executable(gui_forms_row_height_index_tests tests/row_height_index_tests.cpp)
target_link_libraries(gui_forms_row_height_index_tests PRIVATE gui_forms_controls)
# The adapter's private header includes STXlib's.
target_link_libraries(gui_forms_row_height_index_tests PRIVATE stxlib::algorithms)
add_test(NAME gui_forms_row_height_index_tests COMMAND gui_forms_row_height_index_tests)

# The window capsule: its gesture rules and the control.
add_executable(gui_forms_window_capsule_tests tests/window_capsule_tests.cpp)
target_link_libraries(gui_forms_window_capsule_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_window_capsule_tests COMMAND gui_forms_window_capsule_tests)

add_executable(gui_forms_virtual_rows_view_tests tests/virtual_rows_view_tests.cpp)
target_link_libraries(gui_forms_virtual_rows_view_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_virtual_rows_view_tests COMMAND gui_forms_virtual_rows_view_tests)

add_executable(gui_forms_list_view_tests tests/list_view_tests.cpp)
target_link_libraries(gui_forms_list_view_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_list_view_tests COMMAND gui_forms_list_view_tests)

# Hostile handlers (house style 5, commit then notify; owner decision
# 2026-11-16): handlers that clear or replace the items, detach the
# control, drop its last reference or destroy the window from inside a
# notification. One test per subject, and the last-reference drop as its
# own test, so a finding names its control; the sanitizer tree runs them.
add_executable(gui_forms_hostile_handlers_tests tests/hostile_handlers/hostile_handlers_tests.cpp)
target_link_libraries(gui_forms_hostile_handlers_tests PRIVATE gui_forms_controls)
foreach(_gui_hostile_subject IN ITEMS list_view list_box checked_list_box combo_box tree_view tab_control
        split_container track_bar h_scroll_bar numeric_up_down data_grid_view menu_strip context_menu
        tree_list_view virtual_rows_view window_keys)
    add_test(NAME gui_forms_hostile_handlers_${_gui_hostile_subject}
             COMMAND gui_forms_hostile_handlers_tests --subject ${_gui_hostile_subject} --actions mutate)
    add_test(NAME gui_forms_hostile_handlers_${_gui_hostile_subject}_release
             COMMAND gui_forms_hostile_handlers_tests --subject ${_gui_hostile_subject} --actions release)
endforeach()

add_executable(gui_forms_kept_layout_tests tests/kept_layout_tests.cpp)
target_link_libraries(gui_forms_kept_layout_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_kept_layout_tests COMMAND gui_forms_kept_layout_tests)

add_executable(gui_forms_data_grid_view_tests tests/data_grid_view_tests.cpp)
target_link_libraries(gui_forms_data_grid_view_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_data_grid_view_tests COMMAND gui_forms_data_grid_view_tests)
# The one data engine's million-row workload (cp33); set
# GUI_FORMS_GRID_WORKLOAD_TIMING for each step's time.
add_executable(gui_forms_data_grid_view_workload_tests tests/data_grid_view_workload_tests.cpp)
target_link_libraries(gui_forms_data_grid_view_workload_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_data_grid_view_workload_tests COMMAND gui_forms_data_grid_view_workload_tests)
add_executable(gui_forms_data_grid_view_order_tests tests/data_grid_view_order_tests.cpp)
target_link_libraries(gui_forms_data_grid_view_order_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_data_grid_view_order_tests COMMAND gui_forms_data_grid_view_order_tests)

add_executable(gui_forms_masked_text_box_tests tests/masked_text_box_tests.cpp)
target_link_libraries(gui_forms_masked_text_box_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_masked_text_box_tests COMMAND gui_forms_masked_text_box_tests)
add_executable(gui_forms_rich_text_box_tests tests/rich_text_box_tests.cpp)
target_link_libraries(gui_forms_rich_text_box_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_rich_text_box_tests COMMAND gui_forms_rich_text_box_tests)

add_executable(gui_forms_month_calendar_tests tests/month_calendar_tests.cpp)
target_link_libraries(gui_forms_month_calendar_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_month_calendar_tests COMMAND gui_forms_month_calendar_tests)

add_executable(gui_forms_ribbon_gallery_tests tests/ribbon_gallery_tests.cpp)
target_link_libraries(gui_forms_ribbon_gallery_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_ribbon_gallery_tests COMMAND gui_forms_ribbon_gallery_tests)
add_executable(gui_forms_quick_access_toolbar_tests tests/quick_access_toolbar_tests.cpp)
target_link_libraries(gui_forms_quick_access_toolbar_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_quick_access_toolbar_tests COMMAND gui_forms_quick_access_toolbar_tests)
add_executable(gui_forms_ribbon_tool_tip_tests tests/ribbon_tool_tip_tests.cpp)
target_link_libraries(gui_forms_ribbon_tool_tip_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_ribbon_tool_tip_tests COMMAND gui_forms_ribbon_tool_tip_tests)
add_executable(gui_forms_ribbon_look_tests tests/ribbon_look_tests.cpp)
target_link_libraries(gui_forms_ribbon_look_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_ribbon_look_tests COMMAND gui_forms_ribbon_look_tests)
add_executable(gui_forms_overlay_shadow_tests tests/overlay_shadow_tests.cpp)
target_link_libraries(gui_forms_overlay_shadow_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_overlay_shadow_tests COMMAND gui_forms_overlay_shadow_tests)
add_executable(gui_forms_ribbon_state_tests tests/ribbon_state_tests.cpp)
target_link_libraries(gui_forms_ribbon_state_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_ribbon_state_tests COMMAND gui_forms_ribbon_state_tests)
add_executable(gui_forms_focus_region_tests tests/focus_region_tests.cpp)
target_link_libraries(gui_forms_focus_region_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_focus_region_tests COMMAND gui_forms_focus_region_tests)
add_executable(gui_forms_ribbon_vocabulary_tests tests/ribbon_vocabulary_tests.cpp)
target_link_libraries(gui_forms_ribbon_vocabulary_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_ribbon_vocabulary_tests COMMAND gui_forms_ribbon_vocabulary_tests)
add_executable(gui_forms_control_snapshot_tests tests/control_snapshot_tests.cpp)
target_include_directories(gui_forms_control_snapshot_tests PRIVATE tests)
target_link_libraries(gui_forms_control_snapshot_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_control_snapshot_tests COMMAND gui_forms_control_snapshot_tests)
add_executable(gui_forms_dock_drag_tests tests/dock_drag_tests.cpp)
target_include_directories(gui_forms_dock_drag_tests PRIVATE tests)
target_link_libraries(gui_forms_dock_drag_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_dock_drag_tests COMMAND gui_forms_dock_drag_tests)
add_executable(gui_forms_docking_vocabulary_tests tests/docking_vocabulary_tests.cpp)
target_link_libraries(gui_forms_docking_vocabulary_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_docking_vocabulary_tests COMMAND gui_forms_docking_vocabulary_tests)
add_executable(gui_forms_rules_tests tests/rules_tests.cpp)
target_link_libraries(gui_forms_rules_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_rules_tests COMMAND gui_forms_rules_tests)
add_executable(gui_forms_command_route_tests tests/command_route_tests.cpp)
target_link_libraries(gui_forms_command_route_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_command_route_tests COMMAND gui_forms_command_route_tests)
add_executable(gui_forms_ribbon_tests tests/ribbon_tests.cpp)
target_link_libraries(gui_forms_ribbon_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_ribbon_tests COMMAND gui_forms_ribbon_tests)
add_executable(gui_forms_ribbon_scaling_tests tests/ribbon_scaling_tests.cpp)
target_link_libraries(gui_forms_ribbon_scaling_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_ribbon_scaling_tests COMMAND gui_forms_ribbon_scaling_tests)
add_executable(gui_forms_ribbon_contextual_tests tests/ribbon_contextual_tests.cpp)
target_link_libraries(gui_forms_ribbon_contextual_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_ribbon_contextual_tests COMMAND gui_forms_ribbon_contextual_tests)
add_executable(gui_forms_ribbon_file_area_tests tests/ribbon_file_area_tests.cpp)
target_link_libraries(gui_forms_ribbon_file_area_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_ribbon_file_area_tests COMMAND gui_forms_ribbon_file_area_tests)
add_executable(gui_forms_tool_strip_container_tests tests/tool_strip_container_tests.cpp)
target_link_libraries(gui_forms_tool_strip_container_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_tool_strip_container_tests COMMAND gui_forms_tool_strip_container_tests)
add_executable(gui_forms_binding_navigator_tests tests/binding_navigator_tests.cpp)
target_link_libraries(gui_forms_binding_navigator_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_binding_navigator_tests COMMAND gui_forms_binding_navigator_tests)

add_executable(gui_forms_help_system_tests tests/help_system_tests.cpp)
target_link_libraries(gui_forms_help_system_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_help_system_tests COMMAND gui_forms_help_system_tests)
add_executable(gui_forms_font_dialog_tests tests/font_dialog_tests.cpp)
target_link_libraries(gui_forms_font_dialog_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_font_dialog_tests COMMAND gui_forms_font_dialog_tests)

add_executable(gui_forms_background_worker_tests tests/background_worker_tests.cpp)
target_link_libraries(gui_forms_background_worker_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_background_worker_tests COMMAND gui_forms_background_worker_tests)

add_executable(gui_forms_task_dialog_tests tests/task_dialog_tests.cpp)
target_link_libraries(gui_forms_task_dialog_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_task_dialog_tests COMMAND gui_forms_task_dialog_tests)
add_executable(gui_forms_thread_exception_dialog_tests tests/thread_exception_dialog_tests.cpp)
target_link_libraries(gui_forms_thread_exception_dialog_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_thread_exception_dialog_tests COMMAND gui_forms_thread_exception_dialog_tests)
set_tests_properties(gui_forms_background_worker_tests PROPERTIES TIMEOUT 60)

add_executable(gui_forms_command_chain_tests tests/command_chain_tests.cpp)
target_link_libraries(gui_forms_command_chain_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_command_chain_tests COMMAND gui_forms_command_chain_tests)
add_executable(gui_forms_notify_icon_tests tests/notify_icon_tests.cpp)
target_link_libraries(gui_forms_notify_icon_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_notify_icon_tests COMMAND gui_forms_notify_icon_tests)
add_executable(gui_forms_toast_notification_tests tests/toast_notification_tests.cpp)
target_link_libraries(gui_forms_toast_notification_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_toast_notification_tests COMMAND gui_forms_toast_notification_tests)
add_executable(gui_forms_open_file_dialog_tests tests/open_file_dialog_tests.cpp)
target_link_libraries(gui_forms_open_file_dialog_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_open_file_dialog_tests COMMAND gui_forms_open_file_dialog_tests)
add_executable(gui_forms_save_file_dialog_tests tests/save_file_dialog_tests.cpp)
target_link_libraries(gui_forms_save_file_dialog_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_save_file_dialog_tests COMMAND gui_forms_save_file_dialog_tests)
add_executable(gui_forms_color_dialog_tests tests/color_dialog_tests.cpp)
target_link_libraries(gui_forms_color_dialog_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_color_dialog_tests COMMAND gui_forms_color_dialog_tests)
add_executable(gui_forms_folder_browser_dialog_tests tests/folder_browser_dialog_tests.cpp)
target_link_libraries(gui_forms_folder_browser_dialog_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_folder_browser_dialog_tests COMMAND gui_forms_folder_browser_dialog_tests)
add_executable(gui_forms_message_box_tests tests/message_box_tests.cpp)
target_link_libraries(gui_forms_message_box_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_message_box_tests COMMAND gui_forms_message_box_tests)

add_executable(gui_forms_anatomy_tests tests/anatomy_tests.cpp)
target_link_libraries(gui_forms_anatomy_tests PRIVATE gui_forms_lab)
add_test(NAME gui_forms_anatomy_tests COMMAND gui_forms_anatomy_tests)

add_executable(gui_forms_material_tests tests/material_tests.cpp)
target_link_libraries(gui_forms_material_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_material_tests COMMAND gui_forms_material_tests)

add_executable(gui_forms_theme_tests tests/theme_tests.cpp)
target_link_libraries(gui_forms_theme_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_theme_tests COMMAND gui_forms_theme_tests)

add_executable(gui_forms_composition_controls_tests
    tests/composition_controls_tests.cpp
)
target_link_libraries(gui_forms_composition_controls_tests
    PRIVATE gui_forms_lab
)
add_test(NAME gui_forms_composition_controls_tests
    COMMAND gui_forms_composition_controls_tests)

add_executable(gui_forms_split_container_tests
    tests/split_container_tests.cpp
)
target_link_libraries(gui_forms_split_container_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_split_container_tests
         COMMAND gui_forms_split_container_tests)
add_executable(gui_forms_splitter_tests tests/splitter_tests.cpp)
target_link_libraries(gui_forms_splitter_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_splitter_tests COMMAND gui_forms_splitter_tests)
add_executable(gui_forms_node_graph_tests tests/node_graph_tests.cpp)
target_link_libraries(gui_forms_node_graph_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_node_graph_tests COMMAND gui_forms_node_graph_tests)
add_executable(gui_forms_node_graph_history_tests tests/node_graph_history_tests.cpp)
target_link_libraries(gui_forms_node_graph_history_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_node_graph_history_tests COMMAND gui_forms_node_graph_history_tests)
add_executable(gui_forms_link_router_tests tests/link_router_tests.cpp)
target_link_libraries(gui_forms_link_router_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_link_router_tests COMMAND gui_forms_link_router_tests)
# The graph-space scene pieces (gap G8) and the public NodeCanvas (N1).
add_executable(gui_forms_scene_tests tests/scene_tests.cpp)
target_link_libraries(gui_forms_scene_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_scene_tests COMMAND gui_forms_scene_tests)
add_executable(gui_forms_node_canvas_tests tests/node_canvas_tests.cpp)
target_link_libraries(gui_forms_node_canvas_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_node_canvas_tests COMMAND gui_forms_node_canvas_tests)
add_executable(gui_forms_drag_engine_tests tests/drag_engine_tests.cpp)
target_link_libraries(gui_forms_drag_engine_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_drag_engine_tests COMMAND gui_forms_drag_engine_tests)
add_executable(gui_forms_drag_tracker_tests tests/drag_tracker_tests.cpp)
target_link_libraries(gui_forms_drag_tracker_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_drag_tracker_tests COMMAND gui_forms_drag_tracker_tests)
add_executable(gui_forms_scrub_gesture_tests tests/scrub_gesture_tests.cpp)
target_link_libraries(gui_forms_scrub_gesture_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_scrub_gesture_tests COMMAND gui_forms_scrub_gesture_tests)
add_executable(gui_forms_value_text_tests tests/value_text_tests.cpp)
target_link_libraries(gui_forms_value_text_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_value_text_tests COMMAND gui_forms_value_text_tests)
add_executable(gui_forms_value_bar_tests tests/value_bar_tests.cpp)
target_link_libraries(gui_forms_value_bar_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_value_bar_tests COMMAND gui_forms_value_bar_tests)
add_executable(gui_forms_active_border_tests tests/active_border_tests.cpp)
target_link_libraries(gui_forms_active_border_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_active_border_tests COMMAND gui_forms_active_border_tests)

add_executable(gui_forms_layout_contract_tests
    tests/layout_contract_tests.cpp
)
target_link_libraries(gui_forms_layout_contract_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_layout_contract_tests
         COMMAND gui_forms_layout_contract_tests)

# Stage L, phase L2: the layout style reader, against its conformance
# table and the layout corpus's style attributes.
add_executable(gui_forms_layout_style_tests
    tests/layout_style_tests.cpp
)
target_link_libraries(gui_forms_layout_style_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_layout_style_tests
         COMMAND gui_forms_layout_style_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/layout_corpus)

# Stage L, phase L5: the block flow solver headless, and BlockPanel
# with the 10,000-child counts.
add_executable(gui_forms_block_flow_solver_tests
    tests/block_flow_solver_tests.cpp
)
target_link_libraries(gui_forms_block_flow_solver_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_block_flow_solver_tests
         COMMAND gui_forms_block_flow_solver_tests)

add_executable(gui_forms_block_panel_tests
    tests/block_panel_tests.cpp
)
target_link_libraries(gui_forms_block_panel_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_block_panel_tests
         COMMAND gui_forms_block_panel_tests)

# The ImGui-class demonstrator's I2: ColorPicker and ColorValueEditor's
# drop-down picker.
add_executable(gui_forms_color_picker_tests
    tests/color_picker_tests.cpp
)
target_link_libraries(gui_forms_color_picker_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_color_picker_tests
         COMMAND gui_forms_color_picker_tests)

# The ImGui-class demonstrator's I9: FormLayout's labelled rows.
add_executable(gui_forms_form_layout_tests
    tests/form_layout_tests.cpp
)
target_link_libraries(gui_forms_form_layout_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_form_layout_tests
         COMMAND gui_forms_form_layout_tests)

# Stage L, phase L8a: TextBlock, the CSS text leaf, and the baselines
# the containers align it by.
add_executable(gui_forms_text_block_tests
    tests/text_block_tests.cpp
)
target_link_libraries(gui_forms_text_block_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_text_block_tests
         COMMAND gui_forms_text_block_tests)
if(TARGET gui_forms_text_engine)
    add_executable(gui_forms_text_block_engine_tests
        tests/text_block_engine_tests.cpp
    )
    target_link_libraries(gui_forms_text_block_engine_tests PRIVATE gui_forms_controls gui_forms_text_engine)
    add_test(NAME gui_forms_text_block_engine_tests
             COMMAND gui_forms_text_block_engine_tests "${GUI_FORMS_SOURCE_DIR}/assets/fonts")
endif()

# Paragraph layout into caller storage (chat_stress gap A1): wrapping
# against TextBlock's lines and the shared greedy wrapper, the storage
# results, counted allocations, long words and emoji clusters; and the
# same against the bundled engine.
add_executable(gui_forms_paragraph_layout_tests
    tests/paragraph_layout_tests.cpp
)
target_link_libraries(gui_forms_paragraph_layout_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_paragraph_layout_tests
         COMMAND gui_forms_paragraph_layout_tests)
if(TARGET gui_forms_text_engine)
    add_executable(gui_forms_paragraph_layout_engine_tests
        tests/paragraph_layout_engine_tests.cpp
    )
    target_link_libraries(gui_forms_paragraph_layout_engine_tests PRIVATE gui_forms_controls gui_forms_text_engine)
    add_test(NAME gui_forms_paragraph_layout_engine_tests
             COMMAND gui_forms_paragraph_layout_engine_tests "${GUI_FORMS_SOURCE_DIR}/assets/fonts")
endif()

# Stage L, phase L5: absolute and relative positioning in the three CSS
# containers, layout diagnostics and the containing-block dependency.
add_executable(gui_forms_positioning_tests
    tests/positioning_tests.cpp
)
target_link_libraries(gui_forms_positioning_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_positioning_tests
         COMMAND gui_forms_positioning_tests)

# Stage L, phase L4: the grid solver headless, and GridPanel with
# the 10,000-child counts.
add_executable(gui_forms_grid_solver_tests
    tests/grid_solver_tests.cpp
)
target_link_libraries(gui_forms_grid_solver_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_grid_solver_tests
         COMMAND gui_forms_grid_solver_tests)

add_executable(gui_forms_grid_panel_tests
    tests/grid_panel_tests.cpp
)
target_link_libraries(gui_forms_grid_panel_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_grid_panel_tests
         COMMAND gui_forms_grid_panel_tests)

# Stage L, phase L3: the flex solver headless, and FlexPanel with
# the 10,000-child counts.
add_executable(gui_forms_flex_solver_tests
    tests/flex_solver_tests.cpp
)
target_link_libraries(gui_forms_flex_solver_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_flex_solver_tests
         COMMAND gui_forms_flex_solver_tests)

add_executable(gui_forms_flex_panel_tests
    tests/flex_panel_tests.cpp
)
target_link_libraries(gui_forms_flex_panel_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_flex_panel_tests
         COMMAND gui_forms_flex_panel_tests)

add_executable(gui_forms_layout_panel_tests
    tests/layout_panel_tests.cpp
)
target_link_libraries(gui_forms_layout_panel_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_layout_panel_tests
         COMMAND gui_forms_layout_panel_tests)

add_executable(gui_forms_window_resize_measure_tests
    tests/window_resize_measure_tests.cpp
)
target_link_libraries(gui_forms_window_resize_measure_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_window_resize_measure_tests
         COMMAND gui_forms_window_resize_measure_tests)

add_executable(gui_forms_window_root_slot_tests
    tests/window_root_slot_tests.cpp
)
target_link_libraries(gui_forms_window_root_slot_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_window_root_slot_tests
         COMMAND gui_forms_window_root_slot_tests)

add_executable(gui_forms_window_band_tests
    tests/window_band_tests.cpp
)
target_link_libraries(gui_forms_window_band_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_window_band_tests
         COMMAND gui_forms_window_band_tests)

add_executable(gui_forms_window_chrome_tests
    tests/window_chrome_tests.cpp
)
target_link_libraries(gui_forms_window_chrome_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_window_chrome_tests
         COMMAND gui_forms_window_chrome_tests)

add_executable(gui_forms_focus_restore_tests
    tests/focus_restore_tests.cpp
)
target_link_libraries(gui_forms_focus_restore_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_focus_restore_tests
         COMMAND gui_forms_focus_restore_tests)

add_executable(gui_forms_layout_direction_tests
    tests/layout_direction_tests.cpp
)
target_link_libraries(gui_forms_layout_direction_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_layout_direction_tests
         COMMAND gui_forms_layout_direction_tests)

add_executable(gui_forms_right_to_left_tests
    tests/right_to_left_tests.cpp
)
target_link_libraries(gui_forms_right_to_left_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_right_to_left_tests
         COMMAND gui_forms_right_to_left_tests)

# The layout fixture corpus against checked-in Chromium references
# (notes:planning/stages/L-layout/PLAN.md section 6). The corpus path on the
# command line also selects this test when the corpus changes.
add_executable(gui_forms_layout_corpus_tests
    tests/layout_corpus_tests.cpp
)
target_link_libraries(gui_forms_layout_corpus_tests PRIVATE gui_forms_controls)
# Text fixtures (phase L8b) are laid out through the bundled text engine;
# a build without it reports them as not run, which fails the test.
set(GUI_FORMS_LAYOUT_CORPUS_FONTS)
if(TARGET gui_forms_text_engine)
    target_link_libraries(gui_forms_layout_corpus_tests PRIVATE gui_forms_text_engine)
    target_compile_definitions(gui_forms_layout_corpus_tests PRIVATE GUI_FORMS_LAYOUT_CORPUS_TEXT_ENGINE=1)
    set(GUI_FORMS_LAYOUT_CORPUS_FONTS --fonts ${GUI_FORMS_SOURCE_DIR}/assets/fonts)
endif()
add_test(NAME gui_forms_layout_corpus_tests
         COMMAND gui_forms_layout_corpus_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/layout_corpus
                 ${GUI_FORMS_LAYOUT_CORPUS_FONTS})

add_executable(gui_forms_responsive_track_panel_tests
    tests/responsive_track_panel_tests.cpp
)
target_link_libraries(gui_forms_responsive_track_panel_tests PRIVATE
    gui_forms_controls)
add_test(NAME gui_forms_responsive_track_panel_tests
         COMMAND gui_forms_responsive_track_panel_tests)

add_executable(gui_forms_command_overflow_panel_tests
    tests/command_overflow_panel_tests.cpp
)
target_link_libraries(gui_forms_command_overflow_panel_tests PRIVATE
    gui_forms_controls)
add_test(NAME gui_forms_command_overflow_panel_tests
         COMMAND gui_forms_command_overflow_panel_tests)

add_executable(gui_forms_tab_control_tests
    tests/tab_control_tests.cpp
)
target_link_libraries(gui_forms_tab_control_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_tab_control_tests
         COMMAND gui_forms_tab_control_tests)
add_executable(gui_forms_tab_control_strip_tests tests/tab_control_strip_tests.cpp)
target_link_libraries(gui_forms_tab_control_strip_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_tab_control_strip_tests COMMAND gui_forms_tab_control_strip_tests)

add_executable(gui_forms_focus_scope_tests
    tests/focus_scope_tests.cpp
)
target_link_libraries(gui_forms_focus_scope_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_focus_scope_tests
         COMMAND gui_forms_focus_scope_tests)

add_executable(gui_forms_control_depth_tests tests/control_depth_tests.cpp)
target_link_libraries(gui_forms_control_depth_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_control_depth_tests COMMAND gui_forms_control_depth_tests)

add_executable(gui_forms_animation_tests
    tests/animation_tests.cpp
)
target_link_libraries(gui_forms_animation_tests PRIVATE gui_forms_demo_controls)
add_test(NAME gui_forms_animation_tests
         COMMAND gui_forms_animation_tests)

add_executable(gui_forms_semantic_tests
    tests/semantic_tests.cpp
)
target_link_libraries(gui_forms_semantic_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_semantic_tests COMMAND gui_forms_semantic_tests)
# The semantic halves of the UI Automation provider and text tests'
# checks, on the same fixtures, headless (no host, no foreground).
add_executable(gui_forms_semantic_fixture_tests tests/semantic_fixture_tests.cpp)
target_link_libraries(gui_forms_semantic_fixture_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_semantic_fixture_tests COMMAND gui_forms_semantic_fixture_tests)

# The localizable-literal plugin, dogfooded: a unit with the plugin, one
# without it, a static and a shared library with it. Without the plugin
# the test checks the table reader only.
add_library(gui_forms_localizable_plugin_static STATIC tests/localizable_plugin_static.cpp)
target_link_libraries(gui_forms_localizable_plugin_static PUBLIC gui_forms_controls)
gui_forms_compile_with_localizable_plugin(gui_forms_localizable_plugin_static)
add_library(gui_forms_localizable_plugin_shared SHARED tests/localizable_plugin_shared.cpp)
target_include_directories(gui_forms_localizable_plugin_shared PRIVATE ${GUI_FORMS_SOURCE_DIR}/include)
gui_forms_compile_with_localizable_plugin(gui_forms_localizable_plugin_shared)
add_executable(gui_forms_localizable_plugin_tests
    tests/localizable_plugin_tests.cpp
    tests/localizable_plugin_plain.cpp)
target_link_libraries(gui_forms_localizable_plugin_tests PRIVATE
    gui_forms_localizable_plugin_static gui_forms_localizable_plugin_shared gui_forms_controls)
target_compile_definitions(gui_forms_localizable_plugin_tests PRIVATE
    GUI_FORMS_LOCALIZABLE_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
if(GUI_FORMS_LOCALIZABLE_PLUGIN_PATH)
    target_compile_definitions(gui_forms_localizable_plugin_tests PRIVATE GUI_FORMS_LOCALIZABLE_TEST_PLUGIN)
    gui_forms_compile_with_localizable_plugin(gui_forms_localizable_plugin_tests
        SOURCES tests/localizable_plugin_tests.cpp)
endif()
add_test(NAME gui_forms_localizable_plugin_tests COMMAND gui_forms_localizable_plugin_tests)
# The dump tool reads the same tables from the linked program's file.
get_filename_component(_gf_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
find_program(GUI_FORMS_LLVM_OBJCOPY NAMES llvm-objcopy HINTS "${_gf_compiler_dir}")
if(GUI_FORMS_LOCALIZABLE_PLUGIN_PATH AND GUI_FORMS_LLVM_OBJCOPY AND Python3_EXECUTABLE)
    add_test(NAME gui_forms_localizable_dump_tests
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/dump_localizable_literals.py
                     --objcopy ${GUI_FORMS_LLVM_OBJCOPY} --require Ready --require "Static library text"
                     $<TARGET_FILE:gui_forms_localizable_plugin_tests>)
endif()

add_executable(gui_forms_lifecycle_controls_tests
    tests/lifecycle_controls_tests.cpp
)
target_link_libraries(gui_forms_lifecycle_controls_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_lifecycle_controls_tests
         COMMAND gui_forms_lifecycle_controls_tests)

add_executable(gui_forms_retained_lifetime_tests
    tests/retained_lifetime_tests.cpp
)
target_link_libraries(gui_forms_retained_lifetime_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_retained_lifetime_tests
         COMMAND gui_forms_retained_lifetime_tests)

add_executable(gui_forms_invalidation_damage_tests
    tests/invalidation_damage_tests.cpp
)
target_link_libraries(gui_forms_invalidation_damage_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_invalidation_damage_tests
         COMMAND gui_forms_invalidation_damage_tests)

add_executable(gui_forms_display_chunk_tests
    tests/display_chunk_tests.cpp
)
target_link_libraries(gui_forms_display_chunk_tests PRIVATE gui_forms_controls gui_forms_core)
add_test(NAME gui_forms_display_chunk_tests COMMAND gui_forms_display_chunk_tests)

add_executable(gui_forms_paint_marks_tests
    tests/paint_marks_tests.cpp
)
target_link_libraries(gui_forms_paint_marks_tests PRIVATE gui_forms_controls gui_forms_core)
add_test(NAME gui_forms_paint_marks_tests COMMAND gui_forms_paint_marks_tests)

# The plots package's primitives, headless: numeric checks, golden tick
# tables and display traces, and the envelope's pixel-identity proof.
add_executable(gui_forms_plot_primitives_tests tests/plot_primitives_tests.cpp)
target_link_libraries(gui_forms_plot_primitives_tests PRIVATE gui_forms_plot_primitives)
if(GUI_FORMS_EFFECTS_VERIFIED)
    target_compile_definitions(gui_forms_plot_primitives_tests PRIVATE GUI_FORMS_EFFECT_TEST_VERIFIED)
endif()
add_test(NAME gui_forms_plot_primitives_tests COMMAND gui_forms_plot_primitives_tests)
add_executable(gui_forms_plot_decimation_raster_tests tests/plot_decimation_raster_tests.cpp)
target_link_libraries(gui_forms_plot_decimation_raster_tests PRIVATE gui_forms_plot_primitives)
add_test(NAME gui_forms_plot_decimation_raster_tests COMMAND gui_forms_plot_decimation_raster_tests)

add_executable(gui_forms_splash_baker_tests tests/splash_baker_tests.cpp)
target_link_libraries(gui_forms_splash_baker_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_splash_baker_tests COMMAND gui_forms_splash_baker_tests)
set_tests_properties(gui_forms_splash_baker_tests PROPERTIES TIMEOUT 30)
if(TARGET gui_forms_skia)
    add_executable(gui_forms_skia_splash_bake_tests tests/skia_splash_bake_tests.cpp)
    target_link_libraries(gui_forms_skia_splash_bake_tests PRIVATE gui_forms_skia gui_forms_core)
    add_test(NAME gui_forms_skia_splash_bake_tests COMMAND gui_forms_skia_splash_bake_tests)
    set_tests_properties(gui_forms_skia_splash_bake_tests PROPERTIES TIMEOUT 30)
endif()

add_executable(gui_forms_frame_scheduler_tests
    tests/frame_scheduler_tests.cpp
)
target_link_libraries(gui_forms_frame_scheduler_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_frame_scheduler_tests COMMAND gui_forms_frame_scheduler_tests)

# Housekeeping expectations: a missed wake and a slow paint tick each give
# one telemetry record; a normal run gives none (owner, 2026-11-11).
add_executable(gui_forms_housekeeping_expectation_tests tests/housekeeping_expectation_tests.cpp)
target_link_libraries(gui_forms_housekeeping_expectation_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_housekeeping_expectation_tests COMMAND gui_forms_housekeeping_expectation_tests)

# The fail-fast boundary: each mode must end the process with its report
# (site and exception text) and never print "survived".
add_executable(gui_forms_fail_fast_tests tests/fail_fast_tests.cpp)
# It instantiates STXlib's gate templates itself (invoke_callback_classified).
target_link_libraries(gui_forms_fail_fast_tests PRIVATE gui_forms_host_headless stxlib::boundary)
foreach(_gui_fail_fast_case IN ITEMS
        "direct|test.direct|injected direct failure"
        "ui_timer|scheduler.ui_timer|injected timer failure"
        "motion_listener|motion_clock.listener|injected listener failure"
        "host_dispatch|host_session.dispatch|injected event failure"
        "long_message|test.long|L+END"
        "invariant|test.invariant|an invariant broke")
    string(REPLACE "|" ";" _gui_fail_fast_fields "${_gui_fail_fast_case}")
    list(GET _gui_fail_fast_fields 0 _gui_fail_fast_mode)
    list(GET _gui_fail_fast_fields 1 _gui_fail_fast_site)
    list(GET _gui_fail_fast_fields 2 _gui_fail_fast_text)
    # fail_fast() aborts: SIGABRT on Linux and macOS, which CTest counts as a failure
    # whatever the output says, so the mode runs through cmake/expect_fail_fast.cmake.
    add_test(NAME gui_forms_fail_fast_${_gui_fail_fast_mode}
             COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:gui_forms_fail_fast_tests>
                     -DMODE=${_gui_fail_fast_mode}
                     "-DEXPECTED=GUI.Forms: fatal exception at ${_gui_fail_fast_site}: ${_gui_fail_fast_text}"
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/expect_fail_fast.cmake)
endforeach()
# The report prints the failure snapshot's shadow frames: the gate's own
# frame (kind 7, named by the site) and the event frame the target threw
# in, closed by unwinding before the gate caught it.
add_test(NAME gui_forms_fail_fast_shadow_frames
         COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:gui_forms_fail_fast_tests> -DMODE=shadow_frames
                 "-DEXPECTED=GUI.Forms: shadow frames \\(2 open, 2 recorded\\):[^#]*#0 kind 7  test.frames 0[^#]*#1 kind 3 fail-fast.frame click 0"
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/expect_fail_fast.cmake)
# The report's stack comes from STXlib's walk and names each frame's
# image through describe_address (A2 G2): at least one frame lies in the
# test executable itself (its main and test functions), on every host
# where STXlib's Capability::stack_trace and address_lookup hold (macOS,
# Linux and Windows).
add_test(NAME gui_forms_fail_fast_stack_images
         COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:gui_forms_fail_fast_tests> -DMODE=direct
                 "-DEXPECTED=GUI.Forms: stack:.*#[0-9]+ [^+]*gui_forms_fail_fast_tests[^+]*\\+0x[0-9a-f]+"
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/expect_fail_fast.cmake)

# Borrowed views (borrow sanitizer): `plain` uses fresh views and passes
# in both builds; in the checked build each death mode must end the
# process with its report and never print "survived". The modes cover
# each fault on the child list, a collection read, a back link and a
# call-scope payload (tests/borrowed_view_tests.cpp), then stage 3.
add_executable(gui_forms_borrowed_view_tests tests/borrowed_view_tests.cpp)
target_link_libraries(gui_forms_borrowed_view_tests PRIVATE gui_forms_controls gui_forms_core Threads::Threads)
add_test(NAME gui_forms_borrowed_view_plain COMMAND gui_forms_borrowed_view_tests plain)
set_tests_properties(gui_forms_borrowed_view_plain PROPERTIES PASS_REGULAR_EXPRESSION "borrowed views: ok")
if(GUI_FORMS_CHECKED_BORROWS)
    set(_gui_borrow_stale "a borrowed view was used after a structural change of its owner")
    set(_gui_borrow_dead "a borrowed view was used after its owner was destroyed")
    set(_gui_borrow_thread "a borrowed view was used on another thread than the one it was lent on")
    set(_gui_borrow_range "a borrowed view was indexed past its end")
    set(_gui_borrow_null "a null borrowed pointer was dereferenced")
    foreach(_gui_borrow_case IN ITEMS
            "children|stale" "owner_death|dead" "children_thread|thread" "children_range|range"
            "collection|stale" "collection_death|dead" "collection_thread|thread" "collection_range|range"
            "back_link|stale" "back_link_death|dead" "back_link_thread|thread" "back_link_null|null"
            "payload_death|dead" "payload_thread|thread" "payload_range|range"
            "list_items|stale" "text_store|stale" "layout_workspace|stale" "write_lease|stale"
            "raster_death|dead" "tool_strip_items|stale"
            "inline_style|stale" "paint_text|dead")
        string(REPLACE "|" ";" _gui_borrow_fields "${_gui_borrow_case}")
        list(GET _gui_borrow_fields 0 _gui_borrow_mode)
        list(GET _gui_borrow_fields 1 _gui_borrow_fault)
        # Each mode ends in fail_borrow's abort (cmake/expect_fail_fast.cmake).
        add_test(NAME gui_forms_borrowed_view_${_gui_borrow_mode}
                 COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:gui_forms_borrowed_view_tests>
                         -DMODE=${_gui_borrow_mode}
                         "-DEXPECTED=GUI.Forms borrow sanitizer: ${_gui_borrow_${_gui_borrow_fault}}"
                         -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/expect_fail_fast.cmake)
    endforeach()
endif()

add_executable(gui_forms_program_options_tests tests/program_options_tests.cpp)
target_link_libraries(gui_forms_program_options_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_program_options_tests COMMAND gui_forms_program_options_tests)

# ThreadRecordsEnd, the guard each GUI.Forms thread entry declares first.
add_executable(gui_forms_thread_records_tests tests/thread_records_tests.cpp)
target_link_libraries(gui_forms_thread_records_tests PRIVATE gui_forms_core stxlib::boundary stxlib::state)
add_test(NAME gui_forms_thread_records_tests COMMAND gui_forms_thread_records_tests)

add_executable(gui_forms_shadow_frames_tests tests/shadow_frames_tests.cpp)
target_link_libraries(gui_forms_shadow_frames_tests PRIVATE gui_forms_core stxlib::boundary)
add_test(NAME gui_forms_shadow_frames_tests COMMAND gui_forms_shadow_frames_tests)

# The private STXlib bridge (A2 T0): round trips, invalid UTF-8 refused.
# gui_forms_core brings STXlib's shared state, which an error record
# (a refused validation) numbers itself from.
add_executable(gui_forms_stxlib_bridge_tests tests/stxlib_bridge_tests.cpp)
target_link_libraries(gui_forms_stxlib_bridge_tests PRIVATE gui_forms_core stxlib::boundary)
add_test(NAME gui_forms_stxlib_bridge_tests COMMAND gui_forms_stxlib_bridge_tests)

add_executable(gui_forms_timer_tests tests/timer_tests.cpp)
target_link_libraries(gui_forms_timer_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_timer_tests COMMAND gui_forms_timer_tests)

add_executable(gui_forms_surface_capability_tests tests/surface_capability_tests.cpp)
target_link_libraries(gui_forms_surface_capability_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_surface_capability_tests COMMAND gui_forms_surface_capability_tests)

add_executable(gui_forms_dispatcher_tests tests/dispatcher_tests.cpp)
target_link_libraries(gui_forms_dispatcher_tests PRIVATE
    gui_forms_host_headless
    gui_forms_controls
)
add_test(NAME gui_forms_dispatcher_tests COMMAND gui_forms_dispatcher_tests)
set_tests_properties(gui_forms_dispatcher_tests PROPERTIES TIMEOUT 60)

# ThreadSanitizer's set beyond the C ABI (gap P2): the event stack and
# the sub-surface and live-surface machinery whose threads the C ABI's
# sub-surface table reaches. tools/abi_tsan.py runs the tests labelled
# c_abi or tsan against a tsan test cache (gui_forms' tools/abi_tsan.py
# builds and packs it); the c_abi conformance kit
# already covers the ABI's sub-surface part (gf_conformance_subsurfaces.c).
set_property(TEST
    gui_forms_sub_surface_tests
    gui_forms_live_surface_tests
    gui_forms_live_surface_pacer_tests
    gui_forms_live_surface_idle_wake_tests
    gui_forms_threading_tests
    gui_forms_delegate_event_tests
    gui_forms_owned_event_tests
    gui_forms_event_payload_lifetime_tests
    gui_forms_mouse_event_order_tests
    gui_forms_timer_tests
    gui_forms_dispatcher_tests
    APPEND PROPERTY LABELS tsan)

add_executable(gui_forms_tooltip_tests tests/tooltip_tests.cpp)
target_link_libraries(gui_forms_tooltip_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_tooltip_tests COMMAND gui_forms_tooltip_tests)

add_executable(gui_forms_cut_off_text_tests tests/cut_off_text_tests.cpp)
target_link_libraries(gui_forms_cut_off_text_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_cut_off_text_tests COMMAND gui_forms_cut_off_text_tests)

add_executable(gui_forms_binding_tests tests/binding_tests.cpp)
target_link_libraries(gui_forms_binding_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_binding_tests COMMAND gui_forms_binding_tests)
add_executable(gui_forms_binding_provider_tests tests/binding_provider_tests.cpp)
target_link_libraries(gui_forms_binding_provider_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_binding_provider_tests COMMAND gui_forms_binding_provider_tests)
add_executable(gui_forms_number_format_tests tests/number_format_tests.cpp)
target_link_libraries(gui_forms_number_format_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_number_format_tests COMMAND gui_forms_number_format_tests)

add_executable(gui_forms_validation_tests tests/validation_tests.cpp)
target_link_libraries(gui_forms_validation_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_validation_tests COMMAND gui_forms_validation_tests)

add_executable(gui_forms_guidance_provider_tests
    tests/guidance_provider_tests.cpp)
target_link_libraries(gui_forms_guidance_provider_tests PRIVATE
    gui_forms_controls)
add_test(NAME gui_forms_guidance_provider_tests
         COMMAND gui_forms_guidance_provider_tests)

add_executable(gui_forms_stock_icons_tests tests/stock_icons_tests.cpp)
target_link_libraries(gui_forms_stock_icons_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_stock_icons_tests COMMAND gui_forms_stock_icons_tests)
add_executable(gui_forms_control_locale_tests tests/control_locale_tests.cpp)
target_link_libraries(gui_forms_control_locale_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_control_locale_tests COMMAND gui_forms_control_locale_tests)

add_executable(gui_forms_date_time_picker_tests
    tests/date_time_picker_tests.cpp
)
target_link_libraries(gui_forms_date_time_picker_tests PRIVATE gui_forms_controls)
add_test(NAME gui_forms_date_time_picker_tests
         COMMAND gui_forms_date_time_picker_tests)

add_executable(gui_forms_png_registry_tests tests/png_registry_tests.cpp)
target_link_libraries(gui_forms_png_registry_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_png_registry_tests COMMAND gui_forms_png_registry_tests)

add_executable(gui_forms_headless_trace_tests
    tests/headless_trace_tests.cpp
    tests/support/headless_trace.cpp
)
target_include_directories(gui_forms_headless_trace_tests PRIVATE tests)
target_link_libraries(gui_forms_headless_trace_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_headless_trace_tests COMMAND gui_forms_headless_trace_tests)

# The normative trace (release-1.0 condition 2; tests/support/
# normative_trace.hpp): the headless reference, replayed and written to
# normative/ in the build tree for the host drivers' comparison
# (tools/normative_trace.py compare).
add_executable(gui_forms_normative_trace_tests
    tests/normative_trace_tests.cpp
    tests/support/normative_trace.cpp
)
target_include_directories(gui_forms_normative_trace_tests PRIVATE tests)
target_link_libraries(gui_forms_normative_trace_tests PRIVATE gui_forms_core)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/normative)
add_test(NAME gui_forms_normative_trace_tests
         COMMAND gui_forms_normative_trace_tests --write ${CMAKE_CURRENT_BINARY_DIR}/normative)
set_tests_properties(gui_forms_normative_trace_tests PROPERTIES
    FIXTURES_SETUP gui_forms_normative_reference LABELS "normative_trace")

add_executable(gui_forms_host_protocol_tests tests/host_protocol_tests.cpp)
target_link_libraries(gui_forms_host_protocol_tests PRIVATE
    gui_forms_host_headless
)
add_test(NAME gui_forms_host_protocol_tests COMMAND gui_forms_host_protocol_tests)
add_executable(gui_forms_window_teardown_tests tests/window_teardown_tests.cpp)
target_link_libraries(gui_forms_window_teardown_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_window_teardown_tests COMMAND gui_forms_window_teardown_tests)
add_executable(gui_forms_runtime_windows_tests tests/runtime_windows_tests.cpp)
target_link_libraries(gui_forms_runtime_windows_tests PRIVATE gui_forms_host_headless)
add_test(NAME gui_forms_runtime_windows_tests COMMAND gui_forms_runtime_windows_tests)
add_executable(gui_forms_compositor_backdrop_tests tests/compositor_backdrop_tests.cpp)
target_link_libraries(gui_forms_compositor_backdrop_tests PRIVATE gui_forms_host_headless)
add_test(NAME gui_forms_compositor_backdrop_tests COMMAND gui_forms_compositor_backdrop_tests)
add_executable(gui_forms_relative_pointer_tests tests/relative_pointer_tests.cpp)
target_link_libraries(gui_forms_relative_pointer_tests PRIVATE gui_forms_host_headless)
add_test(NAME gui_forms_relative_pointer_tests COMMAND gui_forms_relative_pointer_tests)
# The relative pointer example (examples/relative_pointer), driven headless.
add_executable(gui_forms_relative_pointer_example_tests tests/relative_pointer_example_tests.cpp
    ${GUI_FORMS_SOURCE_DIR}/examples/relative_pointer/orbit_toy.cpp)
target_include_directories(gui_forms_relative_pointer_example_tests PRIVATE
    ${GUI_FORMS_SOURCE_DIR}/examples/relative_pointer)
target_link_libraries(gui_forms_relative_pointer_example_tests PRIVATE gui_forms_host_headless)
add_test(NAME gui_forms_relative_pointer_example_tests COMMAND gui_forms_relative_pointer_example_tests)
add_executable(gui_forms_floating_windows_tests tests/floating_windows_tests.cpp)
target_link_libraries(gui_forms_floating_windows_tests PRIVATE gui_forms_host_headless)
add_test(NAME gui_forms_floating_windows_tests COMMAND gui_forms_floating_windows_tests)
add_executable(gui_forms_native_popup_tests tests/native_popup_tests.cpp)
target_link_libraries(gui_forms_native_popup_tests PRIVATE gui_forms_host_headless gui_forms_controls)
add_test(NAME gui_forms_native_popup_tests COMMAND gui_forms_native_popup_tests)
add_executable(gui_forms_splash_window_tests tests/splash_window_tests.cpp)
target_link_libraries(gui_forms_splash_window_tests PRIVATE gui_forms_host_headless gui_forms_controls)
add_test(NAME gui_forms_splash_window_tests COMMAND gui_forms_splash_window_tests)
add_executable(gui_forms_menu_popup_tests tests/menu_popup_tests.cpp)
target_link_libraries(gui_forms_menu_popup_tests PRIVATE gui_forms_host_headless gui_forms_controls)
add_test(NAME gui_forms_menu_popup_tests COMMAND gui_forms_menu_popup_tests)
add_executable(gui_forms_shared_resources_tests tests/shared_resources_tests.cpp)
target_link_libraries(gui_forms_shared_resources_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_shared_resources_tests COMMAND gui_forms_shared_resources_tests)
add_executable(gui_forms_image_atlas_tests tests/image_atlas_tests.cpp)
target_link_libraries(gui_forms_image_atlas_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_image_atlas_tests COMMAND gui_forms_image_atlas_tests)
add_executable(gui_forms_allocation_statistics_tests tests/allocation_statistics_tests.cpp)
target_link_libraries(gui_forms_allocation_statistics_tests PRIVATE gui_forms_core)
add_test(NAME gui_forms_allocation_statistics_tests COMMAND gui_forms_allocation_statistics_tests)
# GUI.Forms' own code on STXlib's allocator, the program's own allocations
# its own (gui_forms include/gui_forms/allocation/library_memory).
add_executable(gui_forms_library_allocation_tests tests/library_allocation_tests.cpp)
target_link_libraries(gui_forms_library_allocation_tests PRIVATE gui_forms_controls stxlib::boundary)
add_test(NAME gui_forms_library_allocation_tests COMMAND gui_forms_library_allocation_tests)
# The teardown reserve: refusals after the module's teardown began, and the
# fail-fast record when the reserve cannot hold one (a forked child).
add_executable(gui_forms_library_teardown_tests tests/library_teardown_tests.cpp)
target_link_libraries(gui_forms_library_teardown_tests PRIVATE gui_forms_memory stxlib::boundary)
add_test(NAME gui_forms_library_teardown_tests COMMAND gui_forms_library_teardown_tests)
# The opt-in whole-program allocator, compiled in as
# gui_forms_use_library_allocator does.
add_executable(gui_forms_program_allocator_opt_in_tests tests/program_allocator_opt_in_tests.cpp
    ${GUI_FORMS_SOURCE_DIR}/src/runtime/program_allocator/program_allocator_replacement.cpp)
# The replacement uses the teardown reserve of gui_forms_memory, which
# gui_forms_use_library_allocator links; nothing of the engine.
target_link_libraries(gui_forms_program_allocator_opt_in_tests PRIVATE gui_forms_memory stxlib::boundary stxlib::state)
target_compile_features(gui_forms_program_allocator_opt_in_tests PRIVATE cxx_std_20)
add_test(NAME gui_forms_program_allocator_opt_in_tests COMMAND gui_forms_program_allocator_opt_in_tests)
add_executable(gui_forms_dock_floating_tests tests/dock_floating_tests.cpp)
target_link_libraries(gui_forms_dock_floating_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_dock_floating_tests COMMAND gui_forms_dock_floating_tests)
add_executable(gui_forms_workspace_transaction_tests tests/workspace_transaction_tests.cpp)
target_link_libraries(gui_forms_workspace_transaction_tests PRIVATE gui_forms_controls gui_forms_host_headless)
add_test(NAME gui_forms_workspace_transaction_tests COMMAND gui_forms_workspace_transaction_tests)
add_test(
    NAME gui_forms_host_boundary_audit
    COMMAND ${CMAKE_COMMAND}
        -DTARGET_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
        -DNM_TOOL=${CMAKE_NM}
        -DOBJDUMP_TOOL=${CMAKE_OBJDUMP}
        -DCORE_LIBRARY=$<TARGET_FILE:gui_forms_core>
        -DHEADLESS_LIBRARY=$<TARGET_FILE:gui_forms_host_headless>
        -DHEADLESS_TEST_BINARY=$<TARGET_FILE:gui_forms_host_protocol_tests>
        -DPORTABLE_SOURCE_DIRECTORY=${GUI_FORMS_SOURCE_DIR}/src
        -DPUBLIC_INCLUDE_DIRECTORY=${GUI_FORMS_SOURCE_DIR}/include
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_host_boundaries.cmake
)


# The semantic half of the Windows MSAA and UI Automation Gallery
# clients' checks (registry coverage, identities, and one default
# action per control type and role by each client's rules), headless.
if(TARGET gui_forms_gallery_model)
    add_executable(gui_forms_semantic_gallery_tests tests/semantic_gallery_tests.cpp)
    target_link_libraries(gui_forms_semantic_gallery_tests PRIVATE gui_forms_gallery_model)
    add_test(NAME gui_forms_semantic_gallery_tests COMMAND gui_forms_semantic_gallery_tests)
endif()

if(TARGET gui_forms_gallery_model)
add_executable(gui_forms_gallery_interaction_tests
    tests/gallery_interaction_tests.cpp
    )
    target_link_libraries(gui_forms_gallery_interaction_tests PRIVATE
        gui_forms_gallery_model
    )
add_test(NAME gui_forms_gallery_interaction_tests
         COMMAND gui_forms_gallery_interaction_tests)

    add_executable(gui_forms_gallery_pages_tests
        tests/gallery_pages/main.cpp
        tests/gallery_pages/catalog_tests.cpp
        tests/gallery_pages/shell_tests.cpp
        tests/gallery_pages/scroll_extent_tests.cpp
        tests/gallery_pages/display_trace_tests.cpp
        tests/gallery_pages/ranges-and-progress_tests.cpp
        tests/gallery_pages/containers-and-focus_tests.cpp
        tests/gallery_pages/animation-laboratory_tests.cpp
        tests/gallery_pages/text-and-input_tests.cpp
        tests/gallery_pages/collections-and-overlays_tests.cpp
        tests/gallery_pages/values-and-spinners_tests.cpp
        tests/gallery_pages/images-and-drawing_tests.cpp
        tests/gallery_pages/tabs-and-pages_tests.cpp
        tests/gallery_pages/checked-collections_tests.cpp
        tests/gallery_pages/timing-and-tooltips_tests.cpp
        tests/gallery_pages/dates-and-calendar_tests.cpp
        tests/gallery_pages/dialogs-and-host-services_tests.cpp
        tests/gallery_pages/states-and-diagnostics_tests.cpp
        tests/gallery_pages/dock-and-anchor_tests.cpp
        tests/gallery_pages/docking-workspace_tests.cpp
        tests/gallery_pages/retained-layout-panels_tests.cpp
        tests/gallery_pages/css-layout-panels_tests.cpp
        tests/gallery_pages/control-spectrum_tests.cpp
        tests/gallery_pages/commands-and-menus_tests.cpp
        tests/gallery_pages/tree-and-object-views_tests.cpp
        tests/gallery_pages/choices-and-sections_tests.cpp
        tests/gallery_pages/value-editors_tests.cpp
        tests/gallery_pages/responsive-layouts_tests.cpp
        tests/gallery_pages/anchored-overlays_tests.cpp
        tests/gallery_pages/live-surfaces_tests.cpp
        tests/gallery_pages/typography-and-inspection_tests.cpp
        tests/gallery_pages/semantic-feedback_tests.cpp
        tests/gallery_pages/notices-and-badges_tests.cpp
        tests/gallery_pages/input-and-rules_tests.cpp
        tests/gallery_pages/theme-settings_tests.cpp
        tests/gallery_pages/spinners-and-rings_tests.cpp
        tests/gallery_pages/instruments_tests.cpp
        tests/gallery_pages/guided-tasks_tests.cpp
        tests/gallery_pages/help-book_tests.cpp
        tests/gallery_pages/industrial-surfaces_tests.cpp
        tests/gallery_pages/node-graphs_tests.cpp
        tests/gallery_pages/composite-controls_tests.cpp
        tests/gallery_pages/devexpress-demos_tests.cpp
        tests/gallery_pages/anatomy_coverage_tests.cpp
        tests/gallery_pages/schema_export_tests.cpp
    )
    # The theme switch test compiles the Gallery's embedded theme package
    # (gui_forms_add_theme writes its header there: in the gui_forms tree the
    # cache was packed from, which carries it).
    target_include_directories(gui_forms_gallery_pages_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${GUI_FORMS_BINARY_DIR}/gui_forms_themes
    )
    target_link_libraries(gui_forms_gallery_pages_tests PRIVATE
        gui_forms_gallery_model
        gui_forms_host_headless
    )
    # The M0 measurement on the real Gallery (tools/gallery_dirty_lab.cpp);
    # a measurement program, not a test.
    add_executable(gui_forms_gallery_dirty_lab tools/gallery_dirty_lab.cpp)
    target_include_directories(gui_forms_gallery_dirty_lab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(gui_forms_gallery_dirty_lab PRIVATE gui_forms_gallery_model gui_forms_host_headless)
    add_test(NAME gui_forms_gallery_pages_tests
             COMMAND gui_forms_gallery_pages_tests)
    # The test compares its control-schema export with the committed
    # manifest (stage T1).
    set_tests_properties(gui_forms_gallery_pages_tests PROPERTIES ENVIRONMENT
        "GUI_FORMS_CONTROL_SCHEMA=${GUI_FORMS_SOURCE_DIR}/manifests/control-schema.json")
    # Every Gallery page's display commands against the table recorded
    # before the theme engine (T2 E2). A process of its own, so that no
    # other test makes a control first (see display_trace_tests.cpp).
    add_test(NAME gui_forms_gallery_display_trace_tests
             COMMAND gui_forms_gallery_pages_tests --display-trace)
    # The policy script takes one comma-separated argument; a CMake list
    # would be split into separate COMMAND arguments.
    set(gui_forms_gallery_policy_files
        ${GUI_FORMS_SOURCE_DIR}/src/controls/gallery_pages.cpp
        ${GUI_FORMS_SOURCE_DIR}/src/controls/gallery_pages.hpp
        ${GUI_FORMS_SOURCE_DIR}/src/controls/gallery/pages/page_context.hpp
        ${GUI_FORMS_SOURCE_DIR}/src/controls/gallery/pages/pages.hpp
    )
    foreach(gui_forms_gallery_page_source IN LISTS gui_forms_gallery_page_sources)
        list(APPEND gui_forms_gallery_policy_files
            ${GUI_FORMS_SOURCE_DIR}/${gui_forms_gallery_page_source})
    endforeach()
    string(REPLACE ";" "," gui_forms_gallery_policy_file_list
        "${gui_forms_gallery_policy_files}")
    add_test(
        NAME gui_forms_gallery_pages_public_control_policy
        COMMAND ${CMAKE_COMMAND}
            -DGALLERY_PAGE_FILES=${gui_forms_gallery_policy_file_list}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_gallery_pages_public_controls.cmake
    )
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_EXECUTABLE)
        add_test(
            NAME gui_forms_gallery_catalog_current
            COMMAND ${Python3_EXECUTABLE} ${GUI_FORMS_SOURCE_DIR}/tools/control_registry.py emit-cpp --check
        )
    endif()

# The data-workbench demonstrator (demo/data_workbench): the find panel,
# the sort, the column chooser, layouts, summaries and the dashboard.
if(TARGET gui_forms_data_workbench_model)
    add_executable(gui_forms_data_workbench_tests
        tests/data_workbench_tests.cpp)
    target_include_directories(gui_forms_data_workbench_tests PRIVATE
        ${GUI_FORMS_SOURCE_DIR}/demo)
    target_link_libraries(gui_forms_data_workbench_tests PRIVATE
        gui_forms_data_workbench_model)
    add_test(NAME gui_forms_data_workbench_tests
             COMMAND gui_forms_data_workbench_tests)
endif()

if(TARGET gui_forms_visual_inspector_lab_model)
    add_executable(gui_forms_visual_inspector_lab_tests
        tests/visual_inspector_lab_tests.cpp)
    target_include_directories(gui_forms_visual_inspector_lab_tests PRIVATE
        ${GUI_FORMS_SOURCE_DIR}/demo)
    target_link_libraries(gui_forms_visual_inspector_lab_tests PRIVATE
        gui_forms_visual_inspector_lab_model)
    add_test(NAME gui_forms_visual_inspector_lab_tests
             COMMAND gui_forms_visual_inspector_lab_tests)
endif()

if(TARGET gui_forms_layered_material_lab_model)
    add_executable(gui_forms_layered_material_lab_tests
        tests/layered_material_lab_tests.cpp)
    target_include_directories(gui_forms_layered_material_lab_tests PRIVATE
        ${GUI_FORMS_SOURCE_DIR}/demo)
    target_link_libraries(gui_forms_layered_material_lab_tests PRIVATE
        gui_forms_layered_material_lab_model)
    add_test(NAME gui_forms_layered_material_lab_tests
             COMMAND gui_forms_layered_material_lab_tests)

    if(TARGET gui_forms_skia)
        add_executable(gui_forms_layered_material_raster_tests
            tests/layered_material_raster_tests.cpp)
        target_include_directories(gui_forms_layered_material_raster_tests PRIVATE
            ${GUI_FORMS_SOURCE_DIR}/demo
            ${GUI_FORMS_SOURCE_DIR}/src/render/skia/raster)
        target_link_libraries(gui_forms_layered_material_raster_tests PRIVATE
            gui_forms_layered_material_lab_model
            gui_forms_skia)
        add_test(NAME gui_forms_layered_material_raster_tests
                 COMMAND gui_forms_layered_material_raster_tests)
    endif()
endif()

if(TARGET gui_forms_compositor_lab_model AND TARGET gui_forms_skia)
    # The lab in each compositor state, drawn on the Skia raster every
    # host presents with: grants, fallbacks and the frame's alpha.
    add_executable(gui_forms_compositor_lab_tests
        tests/compositor_lab_tests.cpp)
    target_include_directories(gui_forms_compositor_lab_tests PRIVATE
        ${GUI_FORMS_SOURCE_DIR}/demo
        ${GUI_FORMS_SOURCE_DIR}/src/render/skia/raster)
    target_link_libraries(gui_forms_compositor_lab_tests PRIVATE
        gui_forms_compositor_lab_model
        gui_forms_skia)
    add_test(NAME gui_forms_compositor_lab_tests
             COMMAND gui_forms_compositor_lab_tests)
endif()

if(TARGET gui_forms_process_control_model)
    # The process-control demonstrator's window: its dialog, sheet and strip.
    add_executable(gui_forms_process_control_tests
        tests/process_control_tests.cpp)
    target_link_libraries(gui_forms_process_control_tests PRIVATE
        gui_forms_process_control_model)
    add_test(NAME gui_forms_process_control_tests
             COMMAND gui_forms_process_control_tests)
endif()

if(TARGET gui_forms_custom_chrome_lab_model)
    add_executable(gui_forms_custom_chrome_lab_tests
        tests/custom_chrome_lab_tests.cpp)
    target_include_directories(gui_forms_custom_chrome_lab_tests PRIVATE
        ${GUI_FORMS_SOURCE_DIR}/demo
        ${GUI_FORMS_SOURCE_DIR}/src/host/headless)
    target_link_libraries(gui_forms_custom_chrome_lab_tests PRIVATE
        gui_forms_custom_chrome_lab_model
        gui_forms_host_headless)
    add_test(NAME gui_forms_custom_chrome_lab_tests
             COMMAND gui_forms_custom_chrome_lab_tests)
endif()

if(TARGET gui_forms_code_zoom_model)
    # The code-zoom demonstrator: its C++ language, its zoom, its bench's
    # report and its sheet.
    add_executable(gui_forms_code_zoom_tests
        tests/code_zoom_tests.cpp)
    target_link_libraries(gui_forms_code_zoom_tests PRIVATE
        gui_forms_code_zoom_model)
    add_test(NAME gui_forms_code_zoom_tests
             COMMAND gui_forms_code_zoom_tests)
endif()

if(TARGET gui_forms_typography_scale_lab_model)
    add_executable(gui_forms_typography_scale_lab_tests
        tests/typography_scale_lab_tests.cpp)
    target_include_directories(gui_forms_typography_scale_lab_tests PRIVATE
        ${GUI_FORMS_SOURCE_DIR}/demo)
    target_link_libraries(gui_forms_typography_scale_lab_tests PRIVATE
        gui_forms_typography_scale_lab_model)
    add_test(NAME gui_forms_typography_scale_lab_tests
             COMMAND gui_forms_typography_scale_lab_tests)
endif()

if(TARGET gui_forms_responsive_tracks_lab_model)
    add_executable(gui_forms_responsive_tracks_lab_tests
        tests/responsive_tracks_lab_tests.cpp)
    target_include_directories(gui_forms_responsive_tracks_lab_tests PRIVATE
        ${GUI_FORMS_SOURCE_DIR}/demo)
    target_link_libraries(gui_forms_responsive_tracks_lab_tests PRIVATE
        gui_forms_responsive_tracks_lab_model)
    add_test(NAME gui_forms_responsive_tracks_lab_tests
             COMMAND gui_forms_responsive_tracks_lab_tests)
endif()

if(TARGET gui_forms_connected_controls_lab_model)
    add_executable(gui_forms_connected_controls_lab_tests
        tests/connected_controls_lab_tests.cpp)
    target_include_directories(gui_forms_connected_controls_lab_tests PRIVATE
        ${GUI_FORMS_SOURCE_DIR}/demo)
    target_link_libraries(gui_forms_connected_controls_lab_tests PRIVATE
        gui_forms_connected_controls_lab_model)
    add_test(NAME gui_forms_connected_controls_lab_tests
             COMMAND gui_forms_connected_controls_lab_tests)
endif()

if(TARGET gui_forms_seam_proximity_lab_model)
    add_executable(gui_forms_seam_proximity_lab_tests
        tests/seam_proximity_lab_tests.cpp)
    target_include_directories(gui_forms_seam_proximity_lab_tests PRIVATE
        ${GUI_FORMS_SOURCE_DIR}/demo)
    target_link_libraries(gui_forms_seam_proximity_lab_tests PRIVATE
        gui_forms_seam_proximity_lab_model)
    add_test(NAME gui_forms_seam_proximity_lab_tests
             COMMAND gui_forms_seam_proximity_lab_tests)
endif()
endif()

# The Gallery and the labs come from the test cache as IMPORTED targets, for
# which CMake refuses TARGET_BUNDLE_DIR: their fonts are found from the
# executable's directory, <bundle>/Contents/MacOS.
if(TARGET gui_forms_gallery)
    add_test(
        NAME gui_forms_gallery_font_policy
        COMMAND ${CMAKE_COMMAND}
            -DFONT_DIRECTORY=$<TARGET_FILE_DIR:gui_forms_gallery>/../Resources/fonts
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_gallery_fonts.cmake
    )
endif()

if(TARGET gui_forms_visual_inspector_lab)
    add_test(
        NAME gui_forms_visual_inspector_lab_font_policy
        COMMAND ${CMAKE_COMMAND}
            -DFONT_DIRECTORY=$<TARGET_FILE_DIR:gui_forms_visual_inspector_lab>/../Resources/fonts
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_gallery_fonts.cmake
    )
endif()

if(TARGET gui_forms_layered_material_lab)
    add_test(
        NAME gui_forms_layered_material_lab_font_policy
        COMMAND ${CMAKE_COMMAND}
            -DFONT_DIRECTORY=$<TARGET_FILE_DIR:gui_forms_layered_material_lab>/../Resources/fonts
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_gallery_fonts.cmake
    )
endif()

if(TARGET gui_forms_custom_chrome_lab)
    add_test(
        NAME gui_forms_custom_chrome_lab_font_policy
        COMMAND ${CMAKE_COMMAND}
            -DFONT_DIRECTORY=$<TARGET_FILE_DIR:gui_forms_custom_chrome_lab>/../Resources/fonts
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_gallery_fonts.cmake
    )
endif()

if(TARGET gui_forms_typography_scale_lab)
    add_test(
        NAME gui_forms_typography_scale_lab_font_policy
        COMMAND ${CMAKE_COMMAND}
            -DFONT_DIRECTORY=$<TARGET_FILE_DIR:gui_forms_typography_scale_lab>/../Resources/fonts
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_gallery_fonts.cmake
    )
endif()

if(TARGET gui_forms_responsive_tracks_lab)
    add_test(
        NAME gui_forms_responsive_tracks_lab_font_policy
        COMMAND ${CMAKE_COMMAND}
            -DFONT_DIRECTORY=$<TARGET_FILE_DIR:gui_forms_responsive_tracks_lab>/../Resources/fonts
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_gallery_fonts.cmake
    )
endif()

if(TARGET gui_forms_connected_controls_lab)
    add_test(
        NAME gui_forms_connected_controls_lab_font_policy
        COMMAND ${CMAKE_COMMAND}
            -DFONT_DIRECTORY=$<TARGET_FILE_DIR:gui_forms_connected_controls_lab>/../Resources/fonts
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_gallery_fonts.cmake
    )
endif()

if(TARGET gui_forms_seam_proximity_lab)
    add_test(
        NAME gui_forms_seam_proximity_lab_font_policy
        COMMAND ${CMAKE_COMMAND}
            -DFONT_DIRECTORY=$<TARGET_FILE_DIR:gui_forms_seam_proximity_lab>/../Resources/fonts
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_gallery_fonts.cmake
    )
endif()

if(TARGET gui_forms_host_macos)
    add_executable(gui_forms_macos_clipboard_image_tests tests/macos_clipboard_image_tests.mm)
target_compile_options(gui_forms_macos_clipboard_image_tests PRIVATE -fobjc-arc)
target_link_libraries(gui_forms_macos_clipboard_image_tests PRIVATE gui_forms_host_macos)
add_test(NAME gui_forms_macos_clipboard_image_tests COMMAND gui_forms_macos_clipboard_image_tests)

# The release a drag to other programs gives back to its window when the
# dragging session ends. Not built in a high-integrity configuration.
if(NOT GUI_FORMS_HIGH_INTEGRITY)
    add_executable(gui_forms_macos_drag_release_tests tests/macos_drag_release_tests.mm)
    target_compile_options(gui_forms_macos_drag_release_tests PRIVATE -fobjc-arc)
    target_link_libraries(gui_forms_macos_drag_release_tests PRIVATE gui_forms_host_macos)
    add_test(NAME gui_forms_macos_drag_release_tests COMMAND gui_forms_macos_drag_release_tests)
endif()

# The application menu names the running program, not the Gallery.
add_executable(gui_forms_macos_application_menu_tests tests/macos_application_menu_tests.mm)
target_compile_options(gui_forms_macos_application_menu_tests PRIVATE -fobjc-arc)
target_link_libraries(gui_forms_macos_application_menu_tests PRIVATE gui_forms_host_macos)
target_include_directories(gui_forms_macos_application_menu_tests PRIVATE
    ${GUI_FORMS_SOURCE_DIR}/src/surface/macos/application)
add_test(NAME gui_forms_macos_application_menu_tests COMMAND gui_forms_macos_application_menu_tests)

    # Stage T5, C3's measurement: what the window server composes from a
    # non-opaque window and a behind-window effect view, AppKit only.
    # An application bundle launched through LaunchServices, so that its
    # windows are active as a real application's are; the script prints
    # its JSON lines (kept in CI's LastTest.log evidence) and fails when
    # a material case was read in an inactive window. Serial: it reads
    # the screen.
    add_executable(gui_forms_macos_compositor_probe MACOSX_BUNDLE tests/macos_compositor_probe.mm)
    target_compile_options(gui_forms_macos_compositor_probe PRIVATE -fobjc-arc)
    target_link_libraries(gui_forms_macos_compositor_probe PRIVATE
        "-framework AppKit" "-framework ApplicationServices")
    set_target_properties(gui_forms_macos_compositor_probe PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "T5 Compositor Probe"
        MACOSX_BUNDLE_GUI_IDENTIFIER "local.guiforms.t5-compositor-probe"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.3"
        MACOSX_BUNDLE_BUNDLE_VERSION "1")
    add_test(NAME gui_forms_macos_compositor_probe
        COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/support/run_macos_probe_bundle.sh
            $<TARGET_BUNDLE_DIR:gui_forms_macos_compositor_probe>
            ${CMAKE_CURRENT_BINARY_DIR}/macos_compositor_probe.jsonl)
    set_tests_properties(gui_forms_macos_compositor_probe PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)

    add_executable(gui_forms_macos_exposure_tests tests/macos_exposure_tests.mm)
    target_compile_options(gui_forms_macos_exposure_tests PRIVATE -fobjc-arc)
    target_link_libraries(gui_forms_macos_exposure_tests PRIVATE gui_forms_host_macos)
    add_test(NAME gui_forms_macos_exposure_tests COMMAND gui_forms_macos_exposure_tests)

    # The first-frame show gate (first_frame_show.hpp): the first frame is
    # presented off screen and the window shown with it.
    add_executable(gui_forms_macos_first_frame_show_tests tests/macos_first_frame_show_tests.mm)
    target_compile_options(gui_forms_macos_first_frame_show_tests PRIVATE -fobjc-arc)
    target_link_libraries(gui_forms_macos_first_frame_show_tests PRIVATE gui_forms_host_macos)
    add_test(NAME gui_forms_macos_first_frame_show_tests COMMAND gui_forms_macos_first_frame_show_tests)
    set_tests_properties(gui_forms_macos_first_frame_show_tests PROPERTIES TIMEOUT 30)

    # Audit P13: a PDF recording of the view outlives the raster's frame.
    add_executable(gui_forms_macos_recorded_draw_tests tests/macos_recorded_draw_tests.mm)
    target_compile_options(gui_forms_macos_recorded_draw_tests PRIVATE -fobjc-arc)
    target_link_libraries(gui_forms_macos_recorded_draw_tests PRIVATE gui_forms_host_macos)
    add_test(NAME gui_forms_macos_recorded_draw_tests COMMAND gui_forms_macos_recorded_draw_tests)
    set_tests_properties(gui_forms_macos_recorded_draw_tests PROPERTIES TIMEOUT 60)

    # Stage DP gate on macOS (notes:results/2026-10-25-dp-gate-macos).
    add_executable(gui_forms_macos_damage_rectangles_tests tests/macos_damage_rectangles_tests.mm)
    target_compile_options(gui_forms_macos_damage_rectangles_tests PRIVATE -fobjc-arc)
    target_link_libraries(gui_forms_macos_damage_rectangles_tests PRIVATE gui_forms_host_macos)
    add_test(NAME gui_forms_macos_damage_rectangles_tests COMMAND gui_forms_macos_damage_rectangles_tests)
    set_tests_properties(gui_forms_macos_damage_rectangles_tests PROPERTIES TIMEOUT 60)

    add_executable(gui_forms_macos_scroll_phase_tests tests/macos_scroll_phase_tests.mm)
    target_compile_options(gui_forms_macos_scroll_phase_tests PRIVATE -fobjc-arc)
    target_link_libraries(gui_forms_macos_scroll_phase_tests PRIVATE gui_forms_host_macos)
    add_test(NAME gui_forms_macos_scroll_phase_tests COMMAND gui_forms_macos_scroll_phase_tests)
    set_tests_properties(gui_forms_macos_scroll_phase_tests PROPERTIES TIMEOUT 15 LABELS "mac_conditions")

    add_executable(gui_forms_macos_display_link_tests tests/macos_display_link_tests.mm)
    target_compile_options(gui_forms_macos_display_link_tests PRIVATE -fobjc-arc)
    target_link_libraries(gui_forms_macos_display_link_tests PRIVATE gui_forms_host_macos gui_forms_controls)
    add_test(NAME gui_forms_macos_display_link_tests COMMAND gui_forms_macos_display_link_tests)
    add_test(NAME gui_forms_macos_display_link_overlay_tests COMMAND gui_forms_macos_display_link_tests --overlay)
    set_tests_properties(gui_forms_macos_display_link_tests gui_forms_macos_display_link_overlay_tests PROPERTIES TIMEOUT 45)

    add_executable(gui_forms_macos_text_input_caret_tests tests/macos_text_input_caret_tests.mm)
    target_compile_options(gui_forms_macos_text_input_caret_tests PRIVATE -fobjc-arc)
    target_link_libraries(gui_forms_macos_text_input_caret_tests PRIVATE gui_forms_host_macos gui_forms_controls)
    add_test(NAME gui_forms_macos_text_input_caret_tests COMMAND gui_forms_macos_text_input_caret_tests)
    set_tests_properties(gui_forms_macos_text_input_caret_tests PROPERTIES TIMEOUT 15)

    add_executable(gui_forms_macos_text_composition_tests tests/macos_text_composition_tests.mm)
    target_compile_options(gui_forms_macos_text_composition_tests PRIVATE -fobjc-arc)
    target_link_libraries(gui_forms_macos_text_composition_tests PRIVATE gui_forms_host_macos gui_forms_controls)
    add_test(NAME gui_forms_macos_text_composition_tests COMMAND gui_forms_macos_text_composition_tests)
    set_tests_properties(gui_forms_macos_text_composition_tests PROPERTIES TIMEOUT 15)

    add_executable(gui_forms_macos_idle_visibility_tests tests/macos_idle_visibility_tests.mm)
    target_compile_options(gui_forms_macos_idle_visibility_tests PRIVATE -fobjc-arc)
    target_link_libraries(gui_forms_macos_idle_visibility_tests PRIVATE
        gui_forms_host_macos gui_forms_controls)
    add_test(NAME gui_forms_macos_idle_visibility_tests COMMAND gui_forms_macos_idle_visibility_tests)
    set_tests_properties(gui_forms_macos_idle_visibility_tests PROPERTIES TIMEOUT 15)

add_executable(gui_forms_macos_host_close_tests tests/macos_host_close_tests.mm)
    target_compile_options(gui_forms_macos_host_close_tests PRIVATE -fobjc-arc)
    target_link_libraries(gui_forms_macos_host_close_tests PRIVATE
        gui_forms_host_macos
    )
    add_test(NAME gui_forms_macos_host_close_tests
             COMMAND gui_forms_macos_host_close_tests)

    add_executable(gui_forms_macos_message_dialog_key_tests MACOSX_BUNDLE
        tests/macos_message_dialog_key_tests.mm)
    target_compile_options(gui_forms_macos_message_dialog_key_tests PRIVATE -fobjc-arc)
    target_link_libraries(gui_forms_macos_message_dialog_key_tests PRIVATE
        gui_forms_host_macos)
    # Launched through LaunchServices so the alert can become the key
    # window; see cmake/run_macos_activated_test.cmake.
    add_test(NAME gui_forms_macos_message_dialog_key_tests
             COMMAND ${CMAKE_COMMAND}
                 -DBUNDLE=$<TARGET_BUNDLE_DIR:gui_forms_macos_message_dialog_key_tests>
                 -DOUTPUT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_macos_activated_test.cmake)
    set_tests_properties(gui_forms_macos_message_dialog_key_tests PROPERTIES TIMEOUT 60)

    # Stage A: the NSAccessibility tree of every stock control, read in
    # process (no Accessibility permission needed).
    add_executable(gui_forms_macos_accessibility_tests MACOSX_BUNDLE
        tests/macos_accessibility_tests.mm
        tests/support/accessibility_actions.cpp
        tests/support/macos_accessibility_contract.cpp
        tests/support/stock_control_fixture.cpp)
    target_compile_options(gui_forms_macos_accessibility_tests PRIVATE -fobjc-arc)
    target_include_directories(gui_forms_macos_accessibility_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(gui_forms_macos_accessibility_tests PRIVATE
        gui_forms_host_macos gui_forms_lab gui_forms_demo_controls)
    add_test(NAME gui_forms_macos_accessibility_tests
             COMMAND gui_forms_macos_accessibility_tests)
    set_tests_properties(gui_forms_macos_accessibility_tests PROPERTIES TIMEOUT 60)

    # Stage A: the out-of-process AX client and the app it inspects. The
    # client needs the Accessibility permission; without it it reports
    # unsupported and exits 77, which ctest shows as skipped.
    add_executable(gui_forms_macos_accessibility_fixture_app MACOSX_BUNDLE
        tests/macos_accessibility_fixture_app.mm
        tests/support/stock_control_fixture.cpp)
    target_compile_options(gui_forms_macos_accessibility_fixture_app PRIVATE -fobjc-arc)
    target_include_directories(gui_forms_macos_accessibility_fixture_app PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(gui_forms_macos_accessibility_fixture_app PRIVATE
        gui_forms_host_macos gui_forms_lab gui_forms_demo_controls)
    set_target_properties(gui_forms_macos_accessibility_fixture_app PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "local.guiforms.ax-fixture")
    find_library(GUI_FORMS_APPLICATION_SERVICES ApplicationServices REQUIRED)
    add_executable(gui_forms_macos_accessibility_client_tests
        tests/macos_accessibility_client_tests.mm
        tests/support/accessibility_actions.cpp
        tests/support/macos_accessibility_contract.cpp)
    target_compile_options(gui_forms_macos_accessibility_client_tests PRIVATE -fobjc-arc)
    target_include_directories(gui_forms_macos_accessibility_client_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(gui_forms_macos_accessibility_client_tests PRIVATE
        gui_forms_controls ${GUI_FORMS_APPKIT} ${GUI_FORMS_APPLICATION_SERVICES})
    add_dependencies(gui_forms_macos_accessibility_client_tests gui_forms_macos_accessibility_fixture_app)
    add_test(NAME gui_forms_macos_accessibility_client_tests
             COMMAND gui_forms_macos_accessibility_client_tests
                 --app $<TARGET_BUNDLE_DIR:gui_forms_macos_accessibility_fixture_app>
                 --dump ${CMAKE_CURRENT_BINARY_DIR}/accessibility_fixture_semantics.json)
    set_tests_properties(gui_forms_macos_accessibility_client_tests PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 300)
    if(TARGET gui_forms_gallery_model)
        add_executable(gui_forms_macos_accessibility_gallery_app MACOSX_BUNDLE
            tests/macos_accessibility_fixture_app.mm
            tests/support/stock_control_fixture.cpp
            ${GUI_FORMS_PROGRAM_FONT_NOTICES})
        target_compile_options(gui_forms_macos_accessibility_gallery_app PRIVATE -fobjc-arc)
        target_compile_definitions(gui_forms_macos_accessibility_gallery_app PRIVATE GUI_FORMS_AX_GALLERY=1)
        target_include_directories(gui_forms_macos_accessibility_gallery_app PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        target_link_libraries(gui_forms_macos_accessibility_gallery_app PRIVATE
            gui_forms_gallery_model gui_forms_host_macos)
        set_target_properties(gui_forms_macos_accessibility_gallery_app PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "local.guiforms.ax-gallery")
        add_dependencies(gui_forms_macos_accessibility_client_tests gui_forms_macos_accessibility_gallery_app)
        add_test(NAME gui_forms_macos_accessibility_client_gallery_tests
                 COMMAND gui_forms_macos_accessibility_client_tests --gallery
                     --app $<TARGET_BUNDLE_DIR:gui_forms_macos_accessibility_gallery_app>
                     --dump ${CMAKE_CURRENT_BINARY_DIR}/accessibility_gallery_semantics.json)
        # Gallery-wide: walks every page; `ctest -LE gallery_accessibility` skips it.
        set_property(TEST gui_forms_macos_accessibility_client_gallery_tests APPEND PROPERTY LABELS gallery_accessibility)
        set_tests_properties(gui_forms_macos_accessibility_client_gallery_tests PROPERTIES
            SKIP_RETURN_CODE 77 TIMEOUT 600)
    endif()

    add_executable(gui_forms_macos_multi_window_tests
        tests/macos_multi_window_tests.mm)
    target_compile_options(gui_forms_macos_multi_window_tests PRIVATE -fobjc-arc)
    target_link_libraries(gui_forms_macos_multi_window_tests PRIVATE
        gui_forms_host_macos
        gui_forms_controls)
    add_test(NAME gui_forms_macos_multi_window_tests
             COMMAND gui_forms_macos_multi_window_tests)
    # Bundled like applications, carrying the font licences in
    # Resources/fonts; the faces come from the embedded pack.
    set_source_files_properties(
        ${GUI_FORMS_PROGRAM_FONT_NOTICES}
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/fonts")
    foreach(native_fixture IN ITEMS
            gui_forms_macos_clipboard_image_tests
            gui_forms_macos_exposure_tests
            gui_forms_macos_first_frame_show_tests
            gui_forms_macos_damage_rectangles_tests
            gui_forms_macos_idle_visibility_tests
            gui_forms_macos_host_close_tests
            gui_forms_macos_multi_window_tests
            gui_forms_macos_accessibility_tests
            gui_forms_macos_accessibility_fixture_app)
        set_target_properties(${native_fixture} PROPERTIES MACOSX_BUNDLE TRUE)
        target_sources(${native_fixture} PRIVATE
            ${GUI_FORMS_PROGRAM_FONT_NOTICES})
    endforeach()
    if(TARGET gui_forms_prepared_text_impl)
        add_executable(gui_forms_macos_prepared_text_host_tests MACOSX_BUNDLE
            tests/macos_prepared_text_host_tests.mm
            ${GUI_FORMS_PROGRAM_FONT_NOTICES})
        target_compile_options(gui_forms_macos_prepared_text_host_tests PRIVATE -fobjc-arc)
        target_link_libraries(gui_forms_macos_prepared_text_host_tests PRIVATE
            gui_forms_host_macos gui_forms_prepared_text_impl)
        add_test(NAME gui_forms_macos_prepared_text_host_tests
            COMMAND gui_forms_macos_prepared_text_host_tests
                "${GUI_FORMS_SOURCE_DIR}/assets/fonts")
        set_tests_properties(gui_forms_macos_prepared_text_host_tests PROPERTIES TIMEOUT 90)
    endif()
endif()

if(TARGET gui_forms_skia)
    add_executable(gui_forms_live_surface_benchmark EXCLUDE_FROM_ALL ${GUI_FORMS_SOURCE_DIR}/benchmarks/live_surface_benchmark.cpp)
    target_link_libraries(gui_forms_live_surface_benchmark PRIVATE gui_forms_skia)
    # The canvas pan workload (felt pattern, gradient, sheet shadow and live
    # surface, panned in 8-unit steps at scales 1 and 2); run by hand.
    add_executable(gui_forms_canvas_pan_benchmark EXCLUDE_FROM_ALL ${GUI_FORMS_SOURCE_DIR}/benchmarks/canvas_pan_benchmark.cpp)
    target_link_libraries(gui_forms_canvas_pan_benchmark PRIVATE gui_forms_skia)
    # Threaded sub-surfaces measurements M1, M2, M4-M9 for the grid's cell
    # region (stage S7); run by hand, not by ctest.
    add_executable(gui_forms_sub_surface_cells_benchmark EXCLUDE_FROM_ALL ${GUI_FORMS_SOURCE_DIR}/benchmarks/sub_surface_cells_benchmark.cpp)
    target_link_libraries(gui_forms_sub_surface_cells_benchmark PRIVATE gui_forms_controls gui_forms_skia)
    add_executable(gui_forms_opaque_live_surface_raster_tests tests/opaque_live_surface_raster_tests.cpp)
    target_link_libraries(gui_forms_opaque_live_surface_raster_tests PRIVATE gui_forms_controls gui_forms_skia)
    add_test(NAME gui_forms_opaque_live_surface_raster_tests COMMAND gui_forms_opaque_live_surface_raster_tests)
    add_executable(gui_forms_canvas_occlusion_raster_tests tests/canvas_occlusion_raster_tests.cpp)
    target_link_libraries(gui_forms_canvas_occlusion_raster_tests PRIVATE gui_forms_controls gui_forms_skia)
    add_test(NAME gui_forms_canvas_occlusion_raster_tests COMMAND gui_forms_canvas_occlusion_raster_tests)
    add_executable(gui_forms_part_composite_identity_tests tests/part_composite_identity_tests.cpp)
    target_link_libraries(gui_forms_part_composite_identity_tests PRIVATE gui_forms_controls gui_forms_skia)
    add_test(NAME gui_forms_part_composite_identity_tests COMMAND gui_forms_part_composite_identity_tests)
    add_executable(gui_forms_ribbon_scale_raster_tests tests/ribbon_scale_raster_tests.cpp
        $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>)
    target_compile_definitions(gui_forms_ribbon_scale_raster_tests PRIVATE
        GUI_FORMS_TEST_FONT_DIRECTORY="${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    target_include_directories(gui_forms_ribbon_scale_raster_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(gui_forms_ribbon_scale_raster_tests PRIVATE gui_forms_gallery_model gui_forms_skia)
    add_test(NAME gui_forms_ribbon_scale_raster_tests COMMAND gui_forms_ribbon_scale_raster_tests)
    add_executable(gui_forms_rotated_text_raster_tests tests/rotated_text_raster_tests.cpp
        $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>)
    target_compile_definitions(gui_forms_rotated_text_raster_tests PRIVATE
        GUI_FORMS_TEST_FONT_DIRECTORY="${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    target_include_directories(gui_forms_rotated_text_raster_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(gui_forms_rotated_text_raster_tests PRIVATE gui_forms_controls gui_forms_skia)
    add_test(NAME gui_forms_rotated_text_raster_tests COMMAND gui_forms_rotated_text_raster_tests)
    add_executable(gui_forms_surface_material_kernel_tests tests/surface_material_kernel_tests.cpp)
    target_link_libraries(gui_forms_surface_material_kernel_tests PRIVATE gui_forms_controls gui_forms_skia)
    add_test(NAME gui_forms_surface_material_kernel_tests COMMAND gui_forms_surface_material_kernel_tests)
    if(TARGET gui_forms_text_engine)
        add_executable(gui_forms_text_identity_raster_tests tests/text_identity_raster_tests.cpp)
        target_link_libraries(gui_forms_text_identity_raster_tests PRIVATE gui_forms_controls gui_forms_skia)
        target_compile_definitions(gui_forms_text_identity_raster_tests PRIVATE
            GUI_FORMS_TEST_CONTROL_FONT="${GUI_FORMS_SOURCE_DIR}/assets/fonts/Carlito-Regular.ttf")
        add_test(NAME gui_forms_text_identity_raster_tests COMMAND gui_forms_text_identity_raster_tests)
        add_executable(gui_forms_backplane_replay_identity_tests tests/backplane_replay_identity_tests.cpp)
        target_link_libraries(gui_forms_backplane_replay_identity_tests PRIVATE gui_forms_controls gui_forms_skia)
        target_compile_definitions(gui_forms_backplane_replay_identity_tests PRIVATE
            GUI_FORMS_TEST_CONTENT_FONT="${GUI_FORMS_SOURCE_DIR}/assets/fonts/Carlito-Regular.ttf")
        add_test(NAME gui_forms_backplane_replay_identity_tests COMMAND gui_forms_backplane_replay_identity_tests)
    endif()
    add_executable(gui_forms_retained_draw_cache_raster_tests tests/retained_draw_cache_raster_tests.cpp)
    target_link_libraries(gui_forms_retained_draw_cache_raster_tests PRIVATE gui_forms_controls gui_forms_skia)
    target_compile_definitions(gui_forms_retained_draw_cache_raster_tests PRIVATE
        GUI_FORMS_TEST_CONTROL_FONT="${GUI_FORMS_SOURCE_DIR}/assets/fonts/Carlito-Regular.ttf"
        GUI_FORMS_TEST_FALLBACK_FONT="${GUI_FORMS_SOURCE_DIR}/assets/fonts/NotoEmoji-Regular.ttf")
    add_test(NAME gui_forms_retained_draw_cache_raster_tests COMMAND gui_forms_retained_draw_cache_raster_tests)
    if(TARGET gui_forms_text_engine)
        add_executable(gui_forms_paragraph_glyph_tests tests/paragraph_glyph_tests.cpp)
        target_link_libraries(gui_forms_paragraph_glyph_tests PRIVATE
            gui_forms_controls gui_forms_skia gui_forms_text_engine)
        add_test(NAME gui_forms_paragraph_glyph_tests
                 COMMAND gui_forms_paragraph_glyph_tests "${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    endif()
    add_executable(gui_forms_skia_smoke ${GUI_FORMS_SOURCE_DIR}/src/render/skia/skia_raster_smoke.cpp
        $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>)
    target_compile_definitions(gui_forms_skia_smoke PRIVATE
        GUI_FORMS_TEST_CONTROL_FONT="${GUI_FORMS_SOURCE_DIR}/assets/fonts/PortsmouthRapids.ttf"
        GUI_FORMS_TEST_CJK_FONT="${GUI_FORMS_SOURCE_DIR}/assets/fonts/NotoSansCJKjp-Subset.otf"
        GUI_FORMS_TEST_EMOJI_FONT="${GUI_FORMS_SOURCE_DIR}/assets/fonts/NotoEmoji-Regular.ttf"
    )
    target_link_libraries(gui_forms_skia_smoke PRIVATE gui_forms_skia)
    add_test(NAME gui_forms_skia_smoke COMMAND gui_forms_skia_smoke)
    # Process memory of the bundled fonts per configuration (item 4, step 6); a report, not a test.
    # A measuring program, kept with the benchmarks (it reads the host's memory counters).
    add_executable(gui_forms_bundled_font_memory_report ${GUI_FORMS_SOURCE_DIR}/benchmarks/bundled_font_memory_report.cpp
        $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>)
    target_link_libraries(gui_forms_bundled_font_memory_report PRIVATE gui_forms_skia)

    # The specimen sheet (stage T4, phase 2): every stock control of the
    # Gallery in its states, light, dark and high contrast, scales 1 and 2,
    # as contact sheets with a hash manifest and pixel contrast. The test
    # captures the first page twice and requires identical pixels; a full
    # run is `gui_forms_specimen_sheet --out <directory> --repeat`.
    if(TARGET gui_forms_gallery_model)
        add_executable(gui_forms_specimen_sheet tests/specimen/specimen_sheet.cpp
            $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>)
        target_include_directories(gui_forms_specimen_sheet PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_compile_definitions(gui_forms_specimen_sheet PRIVATE
            GUI_FORMS_SPECIMEN_FONT_DIR="${GUI_FORMS_SOURCE_DIR}/assets/fonts")
        target_link_libraries(gui_forms_specimen_sheet PRIVATE gui_forms_gallery_model gui_forms_skia)
        add_test(NAME gui_forms_specimen_sheet_tests COMMAND gui_forms_specimen_sheet --smoke)
    endif()

    # Windowless capture (owner request 2026-11-11, the theme palette):
    # OffscreenRaster with the bundled fonts named, Window::capture_control,
    # specimen states, and the stock specimen catalogue's completeness
    # against stock_control_anatomies().
    add_executable(gui_forms_offscreen_capture_tests tests/offscreen_capture_tests.cpp
        ${GUI_FORMS_SOURCE_DIR}/src/runtime/offscreen/offscreen_raster_create.cpp
        $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>)
    target_compile_definitions(gui_forms_offscreen_capture_tests PRIVATE
        GUI_FORMS_TEST_FONT_DIR="${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    target_link_libraries(gui_forms_offscreen_capture_tests PRIVATE gui_forms_controls gui_forms_skia)
    add_test(NAME gui_forms_offscreen_capture_tests COMMAND gui_forms_offscreen_capture_tests)
    # The Gallery's headless page capture (--page <id> --capture <png>
    # --headless; demo/gallery_headless_capture.hpp), used by previews.
    if(TARGET gui_forms_gallery_model)
        add_executable(gui_forms_gallery_headless_capture_tests tests/gallery_headless_capture_tests.cpp
            ${GUI_FORMS_SOURCE_DIR}/src/runtime/offscreen/offscreen_raster_create.cpp
            $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>)
        target_include_directories(gui_forms_gallery_headless_capture_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_compile_definitions(gui_forms_gallery_headless_capture_tests PRIVATE
            GUI_FORMS_TEST_FONT_DIR="${GUI_FORMS_SOURCE_DIR}/assets/fonts")
        target_link_libraries(gui_forms_gallery_headless_capture_tests PRIVATE gui_forms_gallery_model gui_forms_skia)
        add_test(NAME gui_forms_gallery_headless_capture_tests COMMAND gui_forms_gallery_headless_capture_tests)
    endif()

    # Skeudoindustrial surfaces follow-ups 4 and 5: the indicator lamp
    # and the danger key, their timing, and that each lights only its
    # own lens or face (offscreen raster). GUI_FORMS_PREVIEW_DIR, when
    # set, receives preview shots.
    # The single-surface window manager driven headless by a test host:
    # composing, frames, states, focus and modal blocking.
    add_executable(gui_forms_surface_window_manager_tests tests/surface_window_manager_tests.cpp
        $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>)
    target_compile_definitions(gui_forms_surface_window_manager_tests PRIVATE
        GUI_FORMS_TEST_FONT_DIR="${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    target_link_libraries(gui_forms_surface_window_manager_tests PRIVATE gui_forms_host_surface gui_forms_controls
        gui_forms_skia)
    add_test(NAME gui_forms_surface_window_manager_tests COMMAND gui_forms_surface_window_manager_tests)
    add_executable(gui_forms_indicator_lamp_tests tests/indicator_lamp_tests.cpp
        ${GUI_FORMS_SOURCE_DIR}/src/runtime/offscreen/offscreen_raster_create.cpp
        $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>)
    target_compile_definitions(gui_forms_indicator_lamp_tests PRIVATE
        GUI_FORMS_TEST_FONT_DIR="${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    target_link_libraries(gui_forms_indicator_lamp_tests PRIVATE gui_forms_controls gui_forms_skia)
    add_test(NAME gui_forms_indicator_lamp_tests COMMAND gui_forms_indicator_lamp_tests)
    # ContentSurface's frame, veil and caption in pixels (cp32 census
    # return): default theme and forced high contrast.
    add_executable(gui_forms_content_surface_raster_tests tests/content_surface_raster_tests.cpp
        ${GUI_FORMS_SOURCE_DIR}/src/runtime/offscreen/offscreen_raster_create.cpp
        $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>)
    target_compile_definitions(gui_forms_content_surface_raster_tests PRIVATE
        GUI_FORMS_TEST_FONT_DIR="${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    target_link_libraries(gui_forms_content_surface_raster_tests PRIVATE gui_forms_controls gui_forms_skia)
    add_test(NAME gui_forms_content_surface_raster_tests COMMAND gui_forms_content_surface_raster_tests)
    # ColorPicker's colour areas as content parts drawn by the themes'
    # content rules (W-7), in pixels.
    add_executable(gui_forms_color_picker_raster_tests tests/color_picker_raster_tests.cpp
        ${GUI_FORMS_SOURCE_DIR}/src/runtime/offscreen/offscreen_raster_create.cpp
        $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>)
    target_compile_definitions(gui_forms_color_picker_raster_tests PRIVATE
        GUI_FORMS_TEST_FONT_DIR="${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    target_link_libraries(gui_forms_color_picker_raster_tests PRIVATE gui_forms_controls gui_forms_skia)
    add_test(NAME gui_forms_color_picker_raster_tests COMMAND gui_forms_color_picker_raster_tests)
    # DataGridView's row_pre_paint and row_post_paint answers, in pixels.
    add_executable(gui_forms_data_grid_view_row_paint_raster_tests tests/data_grid_view_row_paint_raster_tests.cpp
        ${GUI_FORMS_SOURCE_DIR}/src/runtime/offscreen/offscreen_raster_create.cpp
        $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>)
    target_compile_definitions(gui_forms_data_grid_view_row_paint_raster_tests PRIVATE
        GUI_FORMS_TEST_FONT_DIR="${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    target_link_libraries(gui_forms_data_grid_view_row_paint_raster_tests PRIVATE gui_forms_controls gui_forms_skia)
    add_test(NAME gui_forms_data_grid_view_row_paint_raster_tests COMMAND gui_forms_data_grid_view_row_paint_raster_tests)
    add_executable(gui_forms_danger_key_tests tests/danger_key_tests.cpp
        ${GUI_FORMS_SOURCE_DIR}/src/runtime/offscreen/offscreen_raster_create.cpp
        $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>)
    target_compile_definitions(gui_forms_danger_key_tests PRIVATE
        GUI_FORMS_TEST_FONT_DIR="${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    target_link_libraries(gui_forms_danger_key_tests PRIVATE gui_forms_controls gui_forms_skia)
    add_test(NAME gui_forms_danger_key_tests COMMAND gui_forms_danger_key_tests)
    # The node canvas's look in pixels (cp31), and its preview set:
    # gui_forms_canvas_look_raster_tests --preview <directory>.
    add_executable(gui_forms_canvas_look_raster_tests tests/canvas_look_raster_tests.cpp
        ${GUI_FORMS_SOURCE_DIR}/src/runtime/offscreen/offscreen_raster_create.cpp
        $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>)
    target_compile_definitions(gui_forms_canvas_look_raster_tests PRIVATE
        GUI_FORMS_TEST_FONT_DIR="${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    target_link_libraries(gui_forms_canvas_look_raster_tests PRIVATE gui_forms_controls gui_forms_skia)
    add_test(NAME gui_forms_canvas_look_raster_tests COMMAND gui_forms_canvas_look_raster_tests)

    add_executable(gui_forms_node_canvas_raster_tests tests/node_canvas_raster_tests.cpp
        ${GUI_FORMS_SOURCE_DIR}/src/runtime/offscreen/offscreen_raster_create.cpp
        $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>)
    target_compile_definitions(gui_forms_node_canvas_raster_tests PRIVATE
        GUI_FORMS_TEST_FONT_DIR="${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    target_link_libraries(gui_forms_node_canvas_raster_tests PRIVATE gui_forms_controls gui_forms_skia)
    add_test(NAME gui_forms_node_canvas_raster_tests COMMAND gui_forms_node_canvas_raster_tests)

    add_executable(gui_forms_canvas_raster_tests tests/canvas_raster_tests.cpp)
    target_link_libraries(gui_forms_canvas_raster_tests PRIVATE gui_forms_controls gui_forms_skia)
    add_test(NAME gui_forms_canvas_raster_tests COMMAND gui_forms_canvas_raster_tests)
    # draw_image below 1x resamples through stxlib::conv (audit P11).
    add_executable(gui_forms_skia_image_downscale_tests tests/skia_image_downscale_tests.cpp)
    target_link_libraries(gui_forms_skia_image_downscale_tests PRIVATE gui_forms_core gui_forms_skia stxlib::algorithms)
    add_test(NAME gui_forms_skia_image_downscale_tests COMMAND gui_forms_skia_image_downscale_tests)

    add_executable(gui_forms_content_scroll_raster_tests tests/content_scroll_raster_tests.cpp)
    target_link_libraries(gui_forms_content_scroll_raster_tests PRIVATE gui_forms_controls gui_forms_skia)
    add_test(NAME gui_forms_content_scroll_raster_tests COMMAND gui_forms_content_scroll_raster_tests)

    add_executable(gui_forms_skia_edge_snapping_tests tests/skia_edge_snapping_tests.cpp)
    target_link_libraries(gui_forms_skia_edge_snapping_tests PRIVATE gui_forms_skia)
    add_test(NAME gui_forms_skia_edge_snapping_tests COMMAND gui_forms_skia_edge_snapping_tests)

    add_executable(gui_forms_paint_marks_raster_tests tests/paint_marks_raster_tests.cpp)
    target_link_libraries(gui_forms_paint_marks_raster_tests PRIVATE gui_forms_controls gui_forms_skia)
    add_test(NAME gui_forms_paint_marks_raster_tests COMMAND gui_forms_paint_marks_raster_tests)
    add_executable(gui_forms_plot_decimation_skia_tests tests/plot_decimation_skia_tests.cpp)
    target_link_libraries(gui_forms_plot_decimation_skia_tests PRIVATE
        gui_forms_plot_primitives gui_forms_controls gui_forms_skia)
    add_test(NAME gui_forms_plot_decimation_skia_tests COMMAND gui_forms_plot_decimation_skia_tests)

    add_executable(gui_forms_skia_damage_edge_tests tests/skia_damage_edge_tests.cpp)
    target_link_libraries(gui_forms_skia_damage_edge_tests PRIVATE gui_forms_skia)
    add_test(NAME gui_forms_skia_damage_edge_tests COMMAND gui_forms_skia_damage_edge_tests)
    # A resize through several sizes presents what a full repaint draws.
    add_executable(gui_forms_resize_damage_equivalence_tests tests/resize_damage_equivalence_tests.cpp)
    target_link_libraries(gui_forms_resize_damage_equivalence_tests PRIVATE gui_forms_controls gui_forms_skia)
    add_test(NAME gui_forms_resize_damage_equivalence_tests COMMAND gui_forms_resize_damage_equivalence_tests)


    add_test(
        NAME gui_forms_skia_archive_policy
        COMMAND ${CMAKE_COMMAND}
            -DSKIA_ARCHIVE=$<TARGET_FILE:stx_skia::stx_skia>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_skia_archive.cmake
    )
endif()

if(TARGET gui_drawing_skia)
    add_executable(gui_drawing_skia_tests tests/drawing_skia_tests.cpp
        $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>)
    target_compile_definitions(gui_drawing_skia_tests PRIVATE
        GUI_DRAWING_TEST_FONT="${GUI_FORMS_SOURCE_DIR}/assets/fonts/PortsmouthRapids.ttf"
        GUI_DRAWING_TEST_BOLD_FONT="${GUI_FORMS_SOURCE_DIR}/assets/fonts/PortsmouthRapids-Bold.ttf"
        GUI_DRAWING_TEST_ITALIC_FONT="${GUI_FORMS_SOURCE_DIR}/assets/fonts/PortsmouthRapids-Italic.ttf"
        GUI_DRAWING_TEST_ARIMO_FONT="${GUI_FORMS_SOURCE_DIR}/assets/fonts/Arimo-Regular.ttf"
    )
    target_link_libraries(gui_drawing_skia_tests PRIVATE gui_drawing_skia gui_forms_core)
    target_include_directories(gui_drawing_skia_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME gui_drawing_skia_tests COMMAND gui_drawing_skia_tests)

    # The sealed pages as the program's own objects: GNU ld reads the
    # raster archive (which holds them too) before gui_drawing_skia's
    # executor asks for them, and this program links no host archive.
    add_executable(gui_drawing_raster_c_api_tests
        tests/drawing_raster_c_api_tests.cpp
        $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>
    )
    target_link_libraries(gui_drawing_raster_c_api_tests PRIVATE
        gui_drawing_c_api
        gui_drawing_raster_c_api
        stxlib::boundary
        stxlib::state
    )
    add_test(NAME gui_drawing_raster_c_api_tests
             COMMAND gui_drawing_raster_c_api_tests)

    # The C++ seam's execute_recorder (drawing/recorder_raster/), from
    # the static drawing raster archive the Application archive carries.
    add_executable(gui_drawing_raster_cxx_tests
        tests/drawing_raster_cxx_tests.cpp
        $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>)
    target_link_libraries(gui_drawing_raster_cxx_tests PRIVATE
        gui_drawing_skia
        gui_forms_core
        stxlib::boundary
        stxlib::state
    )
    add_test(NAME gui_drawing_raster_cxx_tests
             COMMAND gui_drawing_raster_cxx_tests)
endif()

if(TARGET gui_forms_renderer_benchmark)
    add_test(
        NAME gui_forms_renderer_boundary_audit
        COMMAND ${CMAKE_COMMAND}
            -DCORE_LIBRARY=$<TARGET_FILE:gui_forms_core>
            -DSKIA_ADAPTER=$<TARGET_FILE:gui_forms_skia>
            -DBENCHMARK_BINARY=$<TARGET_FILE:gui_forms_renderer_benchmark>
            -DPUBLIC_INCLUDE_DIRECTORY=${GUI_FORMS_SOURCE_DIR}/include
            -DSKIA_ARCHIVE=$<TARGET_FILE:stx_skia::stx_skia>
            -P ${GUI_FORMS_SOURCE_DIR}/cmake/check_renderer_boundaries.cmake
    )
endif()
