cmake_minimum_required(VERSION 3.25)
# GUI.Forms' tests (owner, 2026-10-29 and 2026-11-17: the tests live apart
# from the library). They never compile GUI.Forms: they link the libraries of
# the test cache that gui_forms.lock names (gui_forms tools/test_cache.py),
# through imported targets of the same names, so editing a test recompiles
# that test alone.
#
# Configure through tools/ci_tests.py, which fetches the cache and passes
# GUI_FORMS_TEST_CACHE, the toolchain file and the settings the cache was
# built with; by hand:
#   python3 tools/fetch_test_cache.py            (prints the cache directory)
#   cmake -S . -B build -G Ninja -C <cache>/settings.cmake -DGUI_FORMS_TEST_CACHE=<cache>
project(GUIFormsTests LANGUAGES C CXX)
if(APPLE)
    enable_language(OBJCXX)
endif()

set(GUI_FORMS_TEST_CACHE "" CACHE PATH "The unpacked gui_forms test cache (tools/fetch_test_cache.py)")
if(NOT EXISTS "${GUI_FORMS_TEST_CACHE}/GUIFormsTestCache.cmake")
    message(FATAL_ERROR "GUI_FORMS_TEST_CACHE must name an unpacked gui_forms test cache "
                        "(tools/fetch_test_cache.py prints one); it is \"${GUI_FORMS_TEST_CACHE}\"")
endif()
file(READ "${GUI_FORMS_TEST_CACHE}/manifest.json" _gf_tests_manifest)
string(JSON _gf_tests_cache_compiler GET "${_gf_tests_manifest}" cxx_compiler_version)
string(JSON _gf_tests_cache_commit GET "${_gf_tests_manifest}" commit)
string(JSON _gf_tests_cache_platform GET "${_gf_tests_manifest}" platform)
string(JSON _gf_tests_cache_kind GET "${_gf_tests_manifest}" kind)
# The tests link the cache's static archives: the compiler must be the one
# that built them.
if(NOT CMAKE_CXX_COMPILER_VERSION STREQUAL _gf_tests_cache_compiler)
    message(FATAL_ERROR "The test cache was built with Clang ${_gf_tests_cache_compiler}; this configure "
                        "uses ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
endif()
message(STATUS "GUI.Forms test cache: ${_gf_tests_cache_commit} ${_gf_tests_cache_platform} (${_gf_tests_cache_kind})")
# The loader re-sets the variables the gui_forms configure left at directory
# scope as GUI.Forms' CI had them, the own runtime's three locations among
# them (Windows), and those would reach try_compile's projects over the cache
# entries. This machine's locations are the cache entries the settings give
# (tools/ci_tests.py remaps them), so where the loader names others, the
# configure includes a copy that names these.
file(READ "${GUI_FORMS_TEST_CACHE}/GUIFormsTestCache.cmake" _gf_tests_loader)
set(_gf_tests_relocated FALSE)
foreach(_gf_tests_setting IN ITEMS GUI_FORMS_OWN_RUNTIME_SOURCE GUI_FORMS_OWN_RUNTIME_PREFIX
                                   GUI_FORMS_LIBCXX_PREFIX)
    if(NOT "$CACHE{${_gf_tests_setting}}" STREQUAL "" AND
       _gf_tests_loader MATCHES "_gf_tc_set\\(${_gf_tests_setting} \\[=*\\[([^]\n]+)\\]=*\\]\\)")
        set(_gf_tests_recorded "${CMAKE_MATCH_1}")
        if(NOT _gf_tests_recorded STREQUAL "$CACHE{${_gf_tests_setting}}")
            message(STATUS "GUI.Forms test cache: ${_gf_tests_setting} ${_gf_tests_recorded} is "
                           "$CACHE{${_gf_tests_setting}} here")
            string(REPLACE "${_gf_tests_recorded}" "$CACHE{${_gf_tests_setting}}" _gf_tests_loader
                   "${_gf_tests_loader}")
            set(_gf_tests_relocated TRUE)
        endif()
    endif()
endforeach()
if(_gf_tests_relocated)
    string(REPLACE "\${CMAKE_CURRENT_LIST_DIR}" "${GUI_FORMS_TEST_CACHE}" _gf_tests_loader "${_gf_tests_loader}")
    file(WRITE "${CMAKE_BINARY_DIR}/GUIFormsTestCache.cmake" "${_gf_tests_loader}")
    include("${CMAKE_BINARY_DIR}/GUIFormsTestCache.cmake")
else()
    include("${GUI_FORMS_TEST_CACHE}/GUIFormsTestCache.cmake")
endif()

# The tests that reach into gui_forms' private sources include them from the
# root of the gui_forms tree ("src/...", "demo/...", "lab/..."), which the
# cache carries as its source tree.
include_directories("${GUI_FORMS_SOURCE_DIR}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Register the hostile-macro conformance objects as tests
# (cmake/HostileMacroConformance.cmake). OFF until they have passed on every
# host; CI builds them in one job, allowed to fail, until then.
option(GUI_FORMS_HOSTILE_MACRO_TESTS "Run the hostile-macro conformance compiles as tests" OFF)
# The tests whose form header the form compiler generates at build time
# (gui_forms_add_form). The state-machine gate turns them off: it parses
# sources syntax-only and builds only generated headers.
# Every compile warning in a test, a fixture or a lab fails its build
# (owner, 2026-11-17); ci_tests.py keeps going past failures, so one build
# lists them all.
option(GUI_FORMS_TESTS_WARNINGS_AS_ERRORS "Compile the tests with -Werror" ON)
option(GUI_FORMS_BUILD_GENERATED_FORM_TESTS "Build the tests that generate a form header at build time" ON)

# The functions the tests call that gui_forms defines (from the cache's
# source tree): forms and controls, themes, PE tables and the
# localizable-literal plugin; the fuzz-test functions are this tree's
# (cmake/FuzzTargets.cmake). The plugin is the one gui_forms built (its path
# is the cache's); this configure does not build it again.
include("${GUI_FORMS_SOURCE_DIR}/cmake/GUIFormsForms.cmake")
include("${GUI_FORMS_SOURCE_DIR}/cmake/GUIFormsThemes.cmake")
include(cmake/FuzzTargets.cmake)
include("${GUI_FORMS_SOURCE_DIR}/cmake/PeTables.cmake")
set(_gf_tests_plugin_path "${GUI_FORMS_LOCALIZABLE_PLUGIN_PATH}")
set(_gf_tests_plugin "${GUI_FORMS_LOCALIZABLE_PLUGIN}")
set(_gf_tests_effects_verified "${GUI_FORMS_EFFECTS_VERIFIED}")
set(GUI_FORMS_LOCALIZABLE_PLUGIN OFF)
include("${GUI_FORMS_SOURCE_DIR}/cmake/LocalizablePlugin.cmake")
set(GUI_FORMS_LOCALIZABLE_PLUGIN "${_gf_tests_plugin}")
set(GUI_FORMS_LOCALIZABLE_PLUGIN_PATH "${_gf_tests_plugin_path}")
set(GUI_FORMS_EFFECTS_VERIFIED "${_gf_tests_effects_verified}")

enable_testing()

# The tests, in the order gui_forms' CMakeLists.txt defined them; each
# section was cut from it (or from the cmake module named) on 2026-11-17.
include(cmake/sections/desktop_launchers.cmake)
# A high-integrity cache (gui_forms cmake/HighIntegrity.cmake) leaves out
# entries the suite calls: its tree runs the high-integrity tests alone.
if(GUI_FORMS_HIGH_INTEGRITY)
    include(cmake/sections/high_integrity.cmake)
    include(cmake/sections/first_party_options.cmake)
    return()
endif()
include(cmake/sections/font_pack.cmake)                # cmake/FontPack.cmake
include(cmake/sections/module.cmake)                   # cmake/Module.cmake
include(cmake/sections/file_manager_demoboard.cmake)
include(cmake/sections/windows_drawing_probe.cmake)
include(cmake/sections/macos_live_resize_probe.cmake)
include(cmake/sections/correspondence.cmake)          # demo/correspondence/correspondence.cmake
include(cmake/sections/font_metrics_differential.cmake)
include(cmake/sections/main.cmake)
include(cmake/sections/audio.cmake)                    # cmake/Audio.cmake
include(cmake/sections/audio_fuzz.cmake)
include(cmake/sections/application_text.cmake)         # cmake/ApplicationText.cmake
include(cmake/sections/c_abi.cmake)                    # cmake/CAbi.cmake
include(cmake/sections/high_integrity.cmake)
include(cmake/sections/linux_accessibility.cmake)
include(cmake/sections/windows_application.cmake)
include(cmake/sections/windows_swizzle.cmake)
include(cmake/sections/sdk_consumer.cmake)
include(cmake/sections/c_abi_1.cmake)                  # cmake/CAbi.cmake: the c_abi label
include(cmake/sections/test_areas.cmake)
include(cmake/sections/runtime_and_runners.cmake)
include(cmake/sections/first_party_options.cmake)
include(cmake/sections/reference_arithmetic_warning.cmake)
include(cmake/sections/precompiled_header.cmake)
include(cmake/sections/reference_labels.cmake)
include(cmake/sections/desktop_tests.cmake)
include(cmake/sections/benchmarks.cmake)
include(cmake/sections/fuzzers.cmake)

# The gui_forms tools a test runs from the cache's source tree find this
# checkout (its consumer projects, probes and generated sources) through
# GUI_FORMS_TESTS_ROOT (gui_forms tools/tests_checkout.py); a test script
# finds gui_forms' tools in the cache's source tree through GUI_FORMS_SOURCE_DIR.
# On macOS the window-host tests (label area_host) share the one desktop:
# they order windows front, take key focus and capture the screen, so two at
# once (ctest --parallel, as CI runs) take each other's focus or front place
# (macos_message_dialog_key, macos_compositor_probe and
# macos_damage_rectangles failed that way, and pass one at a time). They take
# the desktop_foreground lock the desktop launchers take; X11 tests each have
# their own Xvfb display.
#
# On Windows a test finds the cache's DLLs (gui_forms_abi1.dll and any other
# shared library the cache imports) through PATH, since nothing stages them
# beside the test programs: CTest prepends each one's directory, the cache's
# bin directory (Shadow, 2026-11-23, question 3).
set(_gf_tests_dll_directories "")
if(WIN32)
    get_property(_gf_tests_imported DIRECTORY PROPERTY IMPORTED_TARGETS)
    foreach(_gf_tests_target IN LISTS _gf_tests_imported)
        get_target_property(_gf_tests_type ${_gf_tests_target} TYPE)
        if(_gf_tests_type STREQUAL "SHARED_LIBRARY")
            foreach(_gf_tests_property IN ITEMS IMPORTED_LOCATION IMPORTED_LOCATION_RELEASE IMPORTED_LOCATION_NOCONFIG)
                get_target_property(_gf_tests_location ${_gf_tests_target} ${_gf_tests_property})
                if(_gf_tests_location)
                    get_filename_component(_gf_tests_directory "${_gf_tests_location}" DIRECTORY)
                    list(APPEND _gf_tests_dll_directories "${_gf_tests_directory}")
                endif()
            endforeach()
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _gf_tests_dll_directories)
endif()
get_property(_gf_tests_all DIRECTORY PROPERTY TESTS)
foreach(_gf_tests_one IN LISTS _gf_tests_all)
    foreach(_gf_tests_directory IN LISTS _gf_tests_dll_directories)
        set_property(TEST ${_gf_tests_one} APPEND PROPERTY ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${_gf_tests_directory}")
    endforeach()
    set_property(TEST ${_gf_tests_one} APPEND PROPERTY ENVIRONMENT
        "GUI_FORMS_TESTS_ROOT=${CMAKE_CURRENT_SOURCE_DIR}" "GUI_FORMS_SOURCE_DIR=${GUI_FORMS_SOURCE_DIR}")
    if(APPLE)
        get_test_property(${_gf_tests_one} LABELS _gf_tests_labels)
        if(_gf_tests_labels AND "area_host" IN_LIST _gf_tests_labels)
            set_property(TEST ${_gf_tests_one} APPEND PROPERTY RESOURCE_LOCK desktop_foreground)
        endif()
    endif()
endforeach()
