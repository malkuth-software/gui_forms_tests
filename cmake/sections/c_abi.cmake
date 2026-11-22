# Only a cache with the C ABI library (gui_forms cmake/CAbi.cmake: GUI_FORMS_BUILD_C_ABI, a native host).
if(TARGET gui_forms_abi)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    get_filename_component(_gf_abi_tool_dir "${CMAKE_C_COMPILER}" DIRECTORY)

    # Version negotiation and the error model, from C11.
    add_executable(gui_forms_abi_runtime_c11_tests tests/abi/gf_abi_runtime_c11_tests.c)
    target_link_libraries(gui_forms_abi_runtime_c11_tests PRIVATE gui_forms_abi)
    # The fixture starts a second thread with pthread_create; CLANG64's libc++
    # uses native Windows threads and brings no pthread dependency of its own.
    if(MINGW)
        target_link_libraries(gui_forms_abi_runtime_c11_tests PRIVATE winpthread)
    else()
        find_package(Threads REQUIRED)
        target_link_libraries(gui_forms_abi_runtime_c11_tests PRIVATE Threads::Threads)
    endif()
    add_test(NAME gui_forms_abi_runtime_c11_tests COMMAND gui_forms_abi_runtime_c11_tests)

    # A C program with no theme code of its own launched with each theme
    # request (--theme, GUI_FORMS_THEME_SHEET), its window on the library's
    # resources, then two on an application's (gui_forms_theme_request_cases
    # in CMakeLists.txt).
    add_executable(gui_forms_abi_theme_request_app tests/abi/gf_abi_theme_request_app.c)
    target_link_libraries(gui_forms_abi_theme_request_app PRIVATE gui_forms_abi)
    add_custom_command(TARGET gui_forms_abi_theme_request_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GUI_FORMS_THEME_REQUEST_FIXTURES}/harbour.css"
            "$<TARGET_FILE_DIR:gui_forms_abi_theme_request_app>/themes/harbour.css")
    gui_forms_theme_request_cases(gui_forms_abi_theme_request gui_forms_abi_theme_request_app)
    gui_forms_theme_request_case(gui_forms_abi_theme_request_application_sheet
        PROGRAM gui_forms_abi_theme_request_app EXPECT applied APPLICATION
        ARGS --theme "${GUI_FORMS_THEME_REQUEST_FIXTURES}/good.css")
    gui_forms_theme_request_case(gui_forms_abi_theme_request_application_replaces
        PROGRAM gui_forms_abi_theme_request_app EXPECT applied APPLICATION POLICY replaces
        ARGS --theme "${GUI_FORMS_THEME_REQUEST_FIXTURES}/good.css")

    # The kit run in-tree, and the check that it calls every slot.
    add_executable(gui_forms_abi_conformance tests/abi/conformance/gf_conformance_main.c)
    target_link_libraries(gui_forms_abi_conformance PRIVATE gui_forms_abi_conformance_kit)
    add_test(NAME gui_forms_abi_conformance COMMAND gui_forms_abi_conformance)
    # Started with no theme request, whatever the caller's environment.
    set_tests_properties(gui_forms_abi_conformance PROPERTIES TIMEOUT 60
                         ENVIRONMENT_MODIFICATION "GUI_FORMS_THEME_SHEET=unset:")
    add_test(NAME gui_forms_abi_slot_coverage
        COMMAND ${Python3_EXECUTABLE} -B ${GUI_FORMS_SOURCE_DIR}/tools/abi_audit.py coverage
            ${GUI_FORMS_SOURCE_DIR}/src/abi/conformance ${CMAKE_CURRENT_SOURCE_DIR}/tests/abi/conformance ${CMAKE_CURRENT_SOURCE_DIR}/tests/abi/schema
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/abi/host)
    # The C ABI library exports exactly its allowlisted gf_ entry points and
    # gd_get_api_v0 (cmake/CheckCAbiExports.cmake against the generated
    # gf_abi_exports.txt: no gdr_, libc++, STXlib or engine symbol, none
    # missing; on Windows the Win32 shim's export names are accepted too). (No
    # foreign text stack is linked since FreeType left on 2026-11-16, so its
    # symbol check is retired.) It is one module: it imports no library but
    # the system's (cmake/CheckSystemImports.cmake).
    set(_gf_abi_extra_exports "")
    if(WIN32)
        set(_gf_abi_extra_exports "-DEXTRA_DEF=${GUI_FORMS_SOURCE_DIR}/src/surface/windows/win32_compat_shim.def")
    endif()
    add_test(NAME gui_forms_abi_exports_only_entry_points
        COMMAND "${CMAKE_COMMAND}" "-DTOOL_NM=${CMAKE_NM}" "-DTOOL_OBJDUMP=${CMAKE_OBJDUMP}"
            "-DLIBRARY=$<TARGET_FILE:gui_forms_abi>"
            "-DEXPECTED=${_gf_abi_generated}/gf_abi_exports.txt"
            ${_gf_abi_extra_exports}
            -P "${GUI_FORMS_SOURCE_DIR}/cmake/CheckCAbiExports.cmake")
    set(_gf_abi_shared_runtime OFF)
    if(APPLE AND NOT GUI_FORMS_STATIC_CXX_RUNTIME)
        set(_gf_abi_shared_runtime ON)
    endif()
    add_test(NAME gui_forms_abi_system_imports_only
        COMMAND "${CMAKE_COMMAND}" "-DTOOL_OBJDUMP=${CMAKE_OBJDUMP}"
            "-DALLOW_SHARED_CXX_RUNTIME=${_gf_abi_shared_runtime}"
            "-DBINARIES=$<TARGET_FILE:gui_forms_abi>"
            -P "${GUI_FORMS_SOURCE_DIR}/cmake/CheckSystemImports.cmake")
    # Windows: no module frees another's memory across the library's boundary
    # (cmake/CheckAllocatorBoundary.cmake). The library and its C and C++
    # consumers export no allocation entry point, take the C heap only from
    # the system CRT and operator new only from their own static libc++, and
    # the consumers import nothing from the library but its entry points and
    # the Win32 shim's names.
    if(WIN32)
        add_test(NAME gui_forms_abi_allocator_boundary
            COMMAND "${CMAKE_COMMAND}" "-DTOOL_OBJDUMP=${CMAKE_OBJDUMP}"
                "-DBINARIES=$<TARGET_FILE:gui_forms_abi>|$<TARGET_FILE:gui_forms_abi_runtime_c11_tests>|$<TARGET_FILE:gui_forms_abi_conformance>|$<TARGET_FILE:gui_forms_abi_schema_export>|$<TARGET_FILE:gui_forms_abi_cpp_wrappers>"
                "-DPROVIDER=$<TARGET_FILE_NAME:gui_forms_abi>"
                "-DPROVIDER_NAMES=${_gf_abi_generated}/gf_abi_exports.txt"
                "-DPROVIDER_DEF=${GUI_FORMS_SOURCE_DIR}/src/surface/windows/win32_compat_shim.def"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckAllocatorBoundary.cmake")
    endif()

    # The stage C3 gate: the control schema read through the ABI alone equals
    # manifests/control-schema.json, its composites loaded from the Gallery's
    # documents (demo/composites) and listed through the schema table. The
    # schema as the ABI gave it is left in the build tree for comparison.
    add_executable(gui_forms_abi_schema_export tests/abi/schema/gf_abi_schema_export.cpp)
    target_link_libraries(gui_forms_abi_schema_export PRIVATE gui_forms_abi)
    target_compile_features(gui_forms_abi_schema_export PRIVATE cxx_std_20)
    add_test(NAME gui_forms_abi_schema_export
        COMMAND gui_forms_abi_schema_export ${GUI_FORMS_SOURCE_DIR}/manifests/control-schema.json
            ${GUI_FORMS_SOURCE_DIR}/manifests/abi/interaction_fields.json
            ${GUI_FORMS_SOURCE_DIR}/demo/composites
            ${CMAKE_CURRENT_BINARY_DIR}/gf_abi/control-schema.abi.json)
    # The stage C4 gate: a theme loaded through the ABI paints a fixture as
    # the C++ path does.
    add_executable(gui_forms_abi_theme_trace tests/abi/themes/gf_abi_theme_trace.cpp)
    target_link_libraries(gui_forms_abi_theme_trace PRIVATE gui_forms_abi_objects gui_forms_application stxlib::boundary)
    target_compile_features(gui_forms_abi_theme_trace PRIVATE cxx_std_20)
    add_test(NAME gui_forms_abi_theme_trace COMMAND gui_forms_abi_theme_trace ${CMAKE_CURRENT_BINARY_DIR}/gf_abi)
    # The host's capability report through the ABI, against the per-host
    # matrix every host must match.
    add_executable(gui_forms_abi_capability_matrix tests/abi/host/gf_abi_capability_matrix.c)
    target_link_libraries(gui_forms_abi_capability_matrix PRIVATE gui_forms_abi)
    add_test(NAME gui_forms_abi_capability_matrix
        COMMAND gui_forms_abi_capability_matrix ${GUI_FORMS_SOURCE_DIR}/manifests/abi/host_capabilities.json)
    # The registry and the ABI's generated type catalog agree.
    add_test(NAME gui_forms_abi_schema_catalog_current
        COMMAND ${Python3_EXECUTABLE} -B ${GUI_FORMS_SOURCE_DIR}/tools/control_registry.py emit-abi --check)
    # The stage C5 fields: C++ events raised directly reach an ABI callback
    # as named fields, and what the callback writes reaches the C++ payload.
    add_executable(gui_forms_abi_event_fields tests/abi/events/gf_abi_event_fields.cpp)
    target_link_libraries(gui_forms_abi_event_fields PRIVATE gui_forms_abi_objects gui_forms_application stxlib::boundary)
    target_compile_features(gui_forms_abi_event_fields PRIVATE cxx_std_20)
    add_test(NAME gui_forms_abi_event_fields COMMAND gui_forms_abi_event_fields)
    # The callback gate (A2, S2 = 1): a client callback runs inside one
    # described frame, and an exception out of it fails fast with the report.
    add_executable(gui_forms_abi_callback_gate tests/abi/callbacks/gf_abi_callback_gate.cpp)
    target_link_libraries(gui_forms_abi_callback_gate PRIVATE gui_forms_abi_objects gui_forms_application stxlib::boundary)
    target_compile_features(gui_forms_abi_callback_gate PRIVATE cxx_std_20)
    add_test(NAME gui_forms_abi_callback_gate COMMAND gui_forms_abi_callback_gate)
    add_test(NAME gui_forms_abi_callback_gate_throwing_event
             COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:gui_forms_abi_callback_gate> -DMODE=throwing_event
                     "-DEXPECTED=GUI.Forms: fatal exception at gui_forms.client_callback: a client callback threw[^#]*#0 kind 3 callback.thrower click"
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/expect_fail_fast.cmake)
    # The checked sanitizer tree poisons an invoke's lent scratch when it
    # returns: a callback's kept text is reported by AddressSanitizer.
    if(GUI_FORMS_CHECKED_BORROWS)
        add_test(NAME gui_forms_abi_event_fields_kept_record
                 COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:gui_forms_abi_event_fields> -DMODE=kept_record
                         "-DEXPECTED=AddressSanitizer: use-after-poison"
                         -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/expect_fail_fast.cmake)
    endif()
    # A custom control's paint callback fills the pixels a window presents.
    add_executable(gui_forms_abi_custom_paint tests/abi/custom/gf_abi_custom_paint.cpp)
    target_link_libraries(gui_forms_abi_custom_paint PRIVATE gui_forms_abi_objects gui_forms_application stxlib::boundary)
    target_compile_features(gui_forms_abi_custom_paint PRIVATE cxx_std_20)
    add_test(NAME gui_forms_abi_custom_paint COMMAND gui_forms_abi_custom_paint)
    # A C++ client's class entry that throws is recorded and ends the process
    # through the fail-fast report of STXlib's callback gate, whose site is
    # gui_forms.client_callback (src/abi/stable/registry/abi_registry.hpp);
    # `plain` checks the program itself runs an entry and survives.
    add_executable(gui_forms_abi_class_entry_fail_fast tests/abi/classes/gf_abi_class_entry_fail_fast.cpp)
    target_link_libraries(gui_forms_abi_class_entry_fail_fast PRIVATE gui_forms_abi_objects gui_forms_application)
    target_compile_features(gui_forms_abi_class_entry_fail_fast PRIVATE cxx_std_20)
    add_test(NAME gui_forms_abi_class_entry_plain COMMAND gui_forms_abi_class_entry_fail_fast plain)
    set_tests_properties(gui_forms_abi_class_entry_plain PROPERTIES PASS_REGULAR_EXPRESSION "class entry: ok")
    add_test(NAME gui_forms_abi_class_entry_fail_fast
             COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:gui_forms_abi_class_entry_fail_fast> -DMODE=throwing_entry
                     "-DEXPECTED=GUI.Forms: fatal exception at gui_forms.client_callback: injected class entry failure"
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/expect_fail_fast.cmake)
    # A set_property callback's kept value text, likewise poisoned in the
    # checked sanitizer tree when the callback returns.
    if(GUI_FORMS_CHECKED_BORROWS)
        add_test(NAME gui_forms_abi_custom_kept_set_text
                 COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:gui_forms_abi_custom_paint> -DMODE=kept_set_text
                         "-DEXPECTED=AddressSanitizer: use-after-poison"
                         -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/expect_fail_fast.cmake)
        add_test(NAME gui_forms_abi_custom_kept_short_set_text
                 COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:gui_forms_abi_custom_paint> -DMODE=kept_short_set_text
                         "-DEXPECTED=AddressSanitizer: use-after-poison"
                         -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/expect_fail_fast.cmake)
    endif()
    add_test(NAME gui_forms_abi_event_catalog_current
        COMMAND ${Python3_EXECUTABLE} -B ${GUI_FORMS_SOURCE_DIR}/tools/abi_event_catalog.py --check)

    # The manifest's sizes and offsets, asserted by this host's compiler in C11 and C++.
    add_executable(gui_forms_abi_layout_probe_c11 tests/abi/gf_abi_layout_probe.c)
    target_include_directories(gui_forms_abi_layout_probe_c11 PRIVATE ${GUI_FORMS_SOURCE_DIR}/include)
    add_test(NAME gui_forms_abi_layout_probe_c11 COMMAND gui_forms_abi_layout_probe_c11)
    add_executable(gui_forms_abi_layout_probe_cpp tests/abi/gf_abi_layout_probe_cpp.cpp)
    target_include_directories(gui_forms_abi_layout_probe_cpp PRIVATE ${GUI_FORMS_SOURCE_DIR}/include)
    add_test(NAME gui_forms_abi_layout_probe_cpp COMMAND gui_forms_abi_layout_probe_cpp)

    # The rules, the current manifest and the current probe.
    add_test(NAME gui_forms_abi_audit
        COMMAND ${Python3_EXECUTABLE} -B ${GUI_FORMS_SOURCE_DIR}/tools/abi_audit.py check
            --clang ${CMAKE_C_COMPILER})
    add_test(NAME gui_forms_abi_layout_probe_current
        COMMAND ${Python3_EXECUTABLE} -B ${GUI_FORMS_SOURCE_DIR}/tools/abi_audit.py probe --check)
    # Every seeded break is reported, and no legal change is.
    add_test(NAME gui_forms_abi_audit_fixtures
        COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/abi/abi_audit_tests.py
            --clang ${CMAKE_C_COMPILER})
    # The built library exports exactly the allowlist it was linked with: the
    # generated list, which adds GUI.Drawing's gd_get_api_v0 to the gf_ entry
    # points (one module), and on Windows the Win32 shim's names.
    set(_gf_abi_export_arguments --library $<TARGET_FILE:gui_forms_abi>
        --allowlist ${_gf_abi_generated}/gf_abi_exports.txt --tool-dir ${_gf_abi_tool_dir})
    if(WIN32)
        list(APPEND _gf_abi_export_arguments --extra-def ${GUI_FORMS_SOURCE_DIR}/src/surface/windows/win32_compat_shim.def)
    endif()
    if(_gf_abi_msvc_import)
        list(APPEND _gf_abi_export_arguments --import-library ${_gf_abi_msvc_import})
    endif()
    add_test(NAME gui_forms_abi_exports
        COMMAND ${Python3_EXECUTABLE} -B ${GUI_FORMS_SOURCE_DIR}/tools/abi_audit.py exports
            ${_gf_abi_export_arguments})
    add_test(NAME gui_forms_abi_audit_exports_tests
        COMMAND ${Python3_EXECUTABLE} -B -m unittest discover -s ${GUI_FORMS_SOURCE_DIR}/tools
            -p test_abi_audit_exports.py)
    # Composite event payloads in the schema speak the C++ events' payload
    # words, each mapped by interaction_fields.json (cp33).
    add_test(NAME gui_forms_abi_composite_payloads
        COMMAND ${Python3_EXECUTABLE} -B -m unittest discover -s ${GUI_FORMS_SOURCE_DIR}/tools
            -p test_composite_payloads.py)
    set_tests_properties(gui_forms_abi_audit gui_forms_abi_audit_fixtures PROPERTIES TIMEOUT 300)

    # A Visual Studio consumer: MSVC compiles the layout probe and the
    # reference consumer against the import library and runs them. It runs
    # where Visual Studio's C++ tools are installed, found through vswhere.
    if(_gf_abi_msvc_import)
        set(_gf_abi_vswhere "$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/Installer/vswhere.exe")
        set(_gf_abi_vs_path "")
        if(EXISTS "${_gf_abi_vswhere}")
            execute_process(COMMAND "${_gf_abi_vswhere}" -latest -products * -requires
                                    Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
                            OUTPUT_VARIABLE _gf_abi_vs_path OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        endif()
        file(TO_CMAKE_PATH "${_gf_abi_vs_path}" _gf_abi_vs_path)
        set(_gf_abi_vcvars "${_gf_abi_vs_path}/VC/Auxiliary/Build/vcvars64.bat")
        if(_gf_abi_vs_path AND EXISTS "${_gf_abi_vcvars}")
            add_test(NAME gui_forms_abi_msvc_consumer
                COMMAND ${CMAKE_COMMAND} -DVCVARS=${_gf_abi_vcvars} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                    -DIMPORT_LIBRARY=${_gf_abi_msvc_import} -DLIBRARY_DIR=$<TARGET_FILE_DIR:gui_forms_abi>
                    -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/gf_abi_msvc_consumer
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/abi/msvc/gf_abi_msvc_consumer.cmake)
            set_tests_properties(gui_forms_abi_msvc_consumer PROPERTIES TIMEOUT 300)
        else()
            message(STATUS "gui_forms_abi_msvc_consumer: no Visual Studio C++ tools; the MSVC consumer test is left out")
        endif()
    endif()

    # The generated C# interop and C++ wrappers are current with the manifest,
    # and the wrappers' slot map is 1:1, on every host.
    add_test(NAME gui_forms_abi_bindings_current
        COMMAND ${Python3_EXECUTABLE} -B ${GUI_FORMS_SOURCE_DIR}/tools/abi_bindings.py csharp --check)
    add_test(NAME gui_forms_abi_cpp_wrappers_current
        COMMAND ${Python3_EXECUTABLE} -B ${GUI_FORMS_SOURCE_DIR}/tools/abi_bindings.py cpp --check)

    # IC3: the behavioural constraint declarations, run. The block table is
    # generated by gui_forms tools/ic_abi_fuzz.py from the declarations'
    # block stubs (tests/ic/gf_abi_ic_blocks.hpp); the target drives it in
    # legal and misuse modes with the public state queries as the oracle,
    # with the hostile-handler compound blocks beside it. The standalone
    # driver sweeps every block, then runs 2000 pseudo-random programs; the
    # libFuzzer build is the same source (GUI_FORMS_BUILD_FUZZERS).
    add_executable(gui_forms_ic_abi_fuzz_tests tests/ic/gf_abi_ic_fuzz.cpp)
    target_link_libraries(gui_forms_ic_abi_fuzz_tests PRIVATE gui_forms_abi_objects gui_forms_application stxlib::boundary)
    target_compile_features(gui_forms_ic_abi_fuzz_tests PRIVATE cxx_std_20)
    target_compile_definitions(gui_forms_ic_abi_fuzz_tests PRIVATE GUI_FORMS_FUZZ_STANDALONE=1)
    add_test(NAME gui_forms_ic_abi_fuzz_tests COMMAND gui_forms_ic_abi_fuzz_tests 2000)
    set_tests_properties(gui_forms_ic_abi_fuzz_tests PROPERTIES TIMEOUT 600
                         ENVIRONMENT_MODIFICATION "GUI_FORMS_THEME_SHEET=unset:")
    if(GUI_FORMS_BUILD_FUZZERS AND COMMAND gui_forms_add_libfuzzer_target)
        gui_forms_add_libfuzzer_target(ic_abi SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/ic/gf_abi_ic_fuzz.cpp
            LIBRARIES gui_forms_abi_objects gui_forms_application stxlib::boundary)
    endif()
    # The block table is current with the declarations, and every declaration
    # with its generated outputs (contract lines, entry checks, the member
    # column, the block stubs, the obligations), and no entry check goes uncalled.
    add_test(NAME gui_forms_ic_abi_blocks_current
        COMMAND ${Python3_EXECUTABLE} -B ${GUI_FORMS_SOURCE_DIR}/tools/ic_abi_fuzz.py --check
            --tests-root ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME gui_forms_ic_declarations_current
        COMMAND ${Python3_EXECUTABLE} -B ${GUI_FORMS_SOURCE_DIR}/tools/ic_declarations.py check
            --config ${GUI_FORMS_SOURCE_DIR}/tools/ic/project.toml --root ${GUI_FORMS_SOURCE_DIR})

    # The C++ wrappers against the built library, linking only the ABI.
    add_executable(gui_forms_abi_cpp_wrappers tests/abi/cpp/gf_abi_cpp_wrappers.cpp)
    target_link_libraries(gui_forms_abi_cpp_wrappers PRIVATE gui_forms_abi)
    target_compile_features(gui_forms_abi_cpp_wrappers PRIVATE cxx_std_20)
    add_test(NAME gui_forms_abi_cpp_wrappers COMMAND gui_forms_abi_cpp_wrappers)

    # The C# interop (bindings/csharp), built offline with the host's .NET SDK
    # and run against this library. Owner ruling (2026-11-02): the test uses a
    # preinstalled SDK of version 10 or later and otherwise skips with a
    # notice in the log; nothing is downloaded or installed for it.
    find_program(GUI_FORMS_DOTNET NAMES dotnet HINTS "$ENV{ProgramFiles}/dotnet" "$ENV{DOTNET_ROOT}")
    # In an AddressSanitizer tree the ASan runtime would be dlopen'ed into the
    # dotnet process, where its interceptors cannot work; the C# checks run in
    # the plain tree, and here the test reports itself skipped.
    set(_gf_abi_csharp_sanitized OFF)
    if(CMAKE_CXX_FLAGS MATCHES "-fsanitize=[^ ]*address")
        set(_gf_abi_csharp_sanitized ON)
    endif()
    add_test(NAME gui_forms_abi_csharp
        COMMAND ${CMAKE_COMMAND} -DDOTNET=${GUI_FORMS_DOTNET} -DPYTHON=${Python3_EXECUTABLE}
            -DSANITIZED=${_gf_abi_csharp_sanitized}
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DGUI_FORMS_DIR=${GUI_FORMS_SOURCE_DIR}
            -DLIBRARY_DIR=$<TARGET_FILE_DIR:gui_forms_abi>
            -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/gf_abi_csharp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/abi/csharp/gf_abi_csharp.cmake)
    set_tests_properties(gui_forms_abi_csharp PROPERTIES TIMEOUT 600
                         SKIP_REGULAR_EXPRESSION "gui_forms_abi_csharp: SKIPPED")
endif()
