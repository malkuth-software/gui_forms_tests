if(WIN32 AND GUI_FORMS_ENABLE_WINDOWS_HOST)
    add_executable(gui_forms_windows_input_settings_tests tests/windows_input_settings_tests.cpp)
    target_link_libraries(gui_forms_windows_input_settings_tests PRIVATE GUIForms::Application)
    add_test(NAME gui_forms_windows_input_settings_tests COMMAND gui_forms_windows_input_settings_tests)
    set_tests_properties(gui_forms_windows_input_settings_tests PROPERTIES TIMEOUT 60)
    # N61: the host places the system input method's windows at the caret and
    # does not advertise text_composition.
    add_executable(gui_forms_windows_input_method_tests tests/windows_input_method_tests.cpp)
    target_compile_definitions(gui_forms_windows_input_method_tests PRIVATE
        ${GUI_FORMS_WINDOWS_DEFINITIONS})
    target_link_libraries(gui_forms_windows_input_method_tests PRIVATE
        gui_forms_controls gui_forms_host_windows imm32)
    add_test(NAME gui_forms_windows_input_method_tests COMMAND gui_forms_windows_input_method_tests)
    set_tests_properties(gui_forms_windows_input_method_tests PROPERTIES TIMEOUT 60)
    # The normative trace on Win32 (tests/windows_normative_trace.cpp), at a
    # fixed scale 1 and 2 (the references' scales), each then compared with
    # the headless reference. A foreground test: the steps are posted to the
    # window's own queue, so a private desktop serves.
    add_executable(gui_forms_windows_normative_trace
        tests/windows_normative_trace.cpp
        tests/support/normative_trace.cpp)
    target_compile_definitions(gui_forms_windows_normative_trace PRIVATE ${GUI_FORMS_WINDOWS_DEFINITIONS})
    target_include_directories(gui_forms_windows_normative_trace PRIVATE tests)
    target_link_libraries(gui_forms_windows_normative_trace PRIVATE gui_forms_controls gui_forms_host_windows)
    add_test(NAME gui_forms_windows_normative_trace
             COMMAND gui_forms_windows_normative_trace --scale 1
                 --output ${CMAKE_CURRENT_BINARY_DIR}/normative/windows.trace)
    add_test(NAME gui_forms_windows_normative_trace_2x
             COMMAND gui_forms_windows_normative_trace --scale 2
                 --output ${CMAKE_CURRENT_BINARY_DIR}/normative/windows-2x.trace)
    set_tests_properties(gui_forms_windows_normative_trace PROPERTIES
        TIMEOUT 60 FIXTURES_REQUIRED gui_forms_normative_reference
        FIXTURES_SETUP gui_forms_normative_windows LABELS "normative_trace")
    # The foreground label and launcher of the 2x run: the launcher is the
    # target's (gui_forms_foreground_test, below, sets it for the 1x run).
    set_tests_properties(gui_forms_windows_normative_trace_2x PROPERTIES
        TIMEOUT 60 FIXTURES_REQUIRED gui_forms_normative_reference
        FIXTURES_SETUP gui_forms_normative_windows_2x LABELS "normative_trace;foreground")
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME gui_forms_windows_normative_trace_compare
                 COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tools/normative_trace.py compare
                     --reference-dir ${CMAKE_CURRENT_BINARY_DIR}/normative
                     --trace ${CMAKE_CURRENT_BINARY_DIR}/normative/windows.trace)
        set_tests_properties(gui_forms_windows_normative_trace_compare PROPERTIES
            FIXTURES_REQUIRED "gui_forms_normative_reference;gui_forms_normative_windows"
            LABELS "normative_trace")
        add_test(NAME gui_forms_windows_normative_trace_2x_compare
                 COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tools/normative_trace.py compare
                     --reference-dir ${CMAKE_CURRENT_BINARY_DIR}/normative
                     --trace ${CMAKE_CURRENT_BINARY_DIR}/normative/windows-2x.trace)
        set_tests_properties(gui_forms_windows_normative_trace_2x_compare PROPERTIES
            FIXTURES_REQUIRED "gui_forms_normative_reference;gui_forms_normative_windows_2x"
            LABELS "normative_trace")
    endif()
    add_executable(gui_forms_windows_accessibility_tests tests/windows_accessibility_tests.cpp)
    target_link_libraries(gui_forms_windows_accessibility_tests PRIVATE GUIForms::Application oleacc uiautomationcore oleaut32 uuid)
    add_test(NAME gui_forms_windows_accessibility_tests COMMAND gui_forms_windows_accessibility_tests)
    # Stage A, A-W2: the UI Automation provider read through IUIAutomation
    # from a client thread.
    add_executable(gui_forms_windows_accessibility_uia_provider_tests
        tests/windows_accessibility_uia_provider_tests.cpp)
    target_compile_definitions(gui_forms_windows_accessibility_uia_provider_tests PRIVATE
        ${GUI_FORMS_WINDOWS_DEFINITIONS})
    target_link_libraries(gui_forms_windows_accessibility_uia_provider_tests PRIVATE
        gui_forms_controls gui_forms_host_windows uiautomationcore ole32 oleaut32 uuid)
    add_test(NAME gui_forms_windows_accessibility_uia_provider_tests
        COMMAND gui_forms_windows_accessibility_uia_provider_tests)
    set_tests_properties(gui_forms_windows_accessibility_uia_provider_tests PROPERTIES TIMEOUT 60)
    # Stage A, A-W3: UI Automation's TextPattern read through IUIAutomation
    # from a client thread.
    add_executable(gui_forms_windows_accessibility_uia_text_tests
        tests/windows_accessibility_uia_text_tests.cpp)
    target_compile_definitions(gui_forms_windows_accessibility_uia_text_tests PRIVATE
        ${GUI_FORMS_WINDOWS_DEFINITIONS})
    target_link_libraries(gui_forms_windows_accessibility_uia_text_tests PRIVATE
        gui_forms_controls gui_forms_host_windows uiautomationcore ole32 oleaut32 uuid)
    add_test(NAME gui_forms_windows_accessibility_uia_text_tests
        COMMAND gui_forms_windows_accessibility_uia_text_tests)
    set_tests_properties(gui_forms_windows_accessibility_uia_text_tests PROPERTIES TIMEOUT 60)
    # Stage A, A-W1: an MSAA client enumerates every Gallery page in a real
    # Win32 window and performs each control's default action.
    if(TARGET gui_forms_gallery_model)
        add_executable(gui_forms_windows_accessibility_msaa_gallery_tests tests/windows_accessibility_msaa_gallery_tests.cpp)
        target_compile_definitions(gui_forms_windows_accessibility_msaa_gallery_tests PRIVATE
            ${GUI_FORMS_WINDOWS_DEFINITIONS})
        target_link_libraries(gui_forms_windows_accessibility_msaa_gallery_tests PRIVATE
            gui_forms_gallery_model gui_forms_host_windows oleacc uiautomationcore oleaut32 uuid)
        add_test(NAME gui_forms_windows_accessibility_msaa_gallery_tests COMMAND gui_forms_windows_accessibility_msaa_gallery_tests)
        # Every page, then one fresh Gallery window per page of actions:
        # about 10 s in a release tree, about 340 s in the sanitizer tree
        # (measured 2026-10-20 on the owner's machine with other builds
        # running), whose ctest budget is 900 s.
        # Gallery-wide: walks every page; `ctest -LE gallery_accessibility` skips it.
        set_property(TEST gui_forms_windows_accessibility_msaa_gallery_tests APPEND PROPERTY LABELS gallery_accessibility)
        set_tests_properties(gui_forms_windows_accessibility_msaa_gallery_tests PROPERTIES TIMEOUT 900)
        # Stage A, A-W3: an IUIAutomation client on a thread of its own
        # enumerates every Gallery page and performs each control's default
        # action through its pattern.
        add_executable(gui_forms_windows_accessibility_uia_gallery_tests tests/windows_accessibility_uia_gallery_tests.cpp)
        target_compile_definitions(gui_forms_windows_accessibility_uia_gallery_tests PRIVATE
            ${GUI_FORMS_WINDOWS_DEFINITIONS})
        target_link_libraries(gui_forms_windows_accessibility_uia_gallery_tests PRIVATE
            gui_forms_gallery_model gui_forms_host_windows uiautomationcore ole32 oleaut32 uuid)
        add_test(NAME gui_forms_windows_accessibility_uia_gallery_tests COMMAND gui_forms_windows_accessibility_uia_gallery_tests)
        # About 49 s in a release tree. In the sanitizer tree it needs about
        # 950 s of CPU time (every provider call takes a fresh snapshot of the
        # page): 741 s and 975 s wall on a lightly loaded machine, and more
        # than 1800 s through ctest on 2026-10-20 while other workspaces'
        # builds left it about 55% of a core. The limit is twice the
        # heaviest load measured.
        # Gallery-wide: walks every page; `ctest -LE gallery_accessibility` skips it.
        set_property(TEST gui_forms_windows_accessibility_uia_gallery_tests APPEND PROPERTY LABELS gallery_accessibility)
        set_tests_properties(gui_forms_windows_accessibility_uia_gallery_tests PROPERTIES TIMEOUT 3600)
    endif()
endif()
