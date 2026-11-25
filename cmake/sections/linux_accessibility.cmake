# The accessibility service adapter is independently testable without a renderer.
# Stage A on Linux: a real AT-SPI client (tests/linux_accessibility_client.py)
# drives the fixture over D-Bus inside its own session bus with the AT-SPI bus
# launcher and registry. It runs under the system Python, which carries
# PyGObject and the Atspi typelib (not a toolcache Python earlier on PATH).
# On CI a missing prerequisite fails the test; elsewhere the client reports
# the platform as unsupported with ctest's skip code, 77.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(GUI_FORMS_SYSTEM_PYTHON3 NAMES python3 PATHS /usr/bin /usr/local/bin NO_DEFAULT_PATH)
    if(NOT GUI_FORMS_SYSTEM_PYTHON3)
        find_program(GUI_FORMS_SYSTEM_PYTHON3 NAMES python3)
    endif()
    set(gui_forms_linux_accessibility_test_ready OFF)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GUI_FORMS_TEST_ATK QUIET IMPORTED_TARGET atk atk-bridge-2.0 gio-2.0)
        find_package(X11 QUIET)
        if(TARGET PkgConfig::GUI_FORMS_TEST_ATK AND TARGET X11::X11)
            add_executable(gui_forms_linux_accessibility_fixture
                tests/linux_accessibility_fixture.cpp
                ${GUI_FORMS_SOURCE_DIR}/src/surface/linux/accessibility/linux_accessibility.cpp
                ${GUI_FORMS_SOURCE_DIR}/src/surface/linux/services/x11_foreign_window.cpp)
            target_link_libraries(gui_forms_linux_accessibility_fixture PRIVATE
                gui_forms_controls gui_forms_core PkgConfig::GUI_FORMS_TEST_ATK X11::X11)
            set(gui_forms_linux_accessibility_test_ready ON)
        endif()
    endif()
    if(NOT GUI_FORMS_SYSTEM_PYTHON3)
        if("$ENV{CI}" STREQUAL "true")
            message(FATAL_ERROR "The Linux accessibility test needs python3 on CI")
        endif()
        message(STATUS "Linux accessibility test: unsupported (no python3)")
    elseif(gui_forms_linux_accessibility_test_ready)
        add_test(NAME gui_forms_linux_accessibility_tests
                 COMMAND ${GUI_FORMS_SYSTEM_PYTHON3} -B
                         ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux_accessibility_client.py
                         $<TARGET_FILE:gui_forms_linux_accessibility_fixture>)
    else()
        add_test(NAME gui_forms_linux_accessibility_tests
                 COMMAND ${GUI_FORMS_SYSTEM_PYTHON3} -B
                         ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux_accessibility_client.py
                         --unsupported "ATK, atk-bridge-2.0 or X11 development files were not found")
    endif()
    if(TEST gui_forms_linux_accessibility_tests)
        set_tests_properties(gui_forms_linux_accessibility_tests PROPERTIES
            SKIP_RETURN_CODE 77)
    endif()
    # A-L2: the whole Gallery through AT-SPI (every stock control's node and
    # default action). The Gallery model exists only with
    # GUI_FORMS_BUILD_GALLERY_MODEL, which the test cache sets; without it
    # there is no Gallery to publish and the test is not configured. It walks
    # every page over D-Bus, so it gets a longer timeout than the suite's.
    if(gui_forms_linux_accessibility_test_ready AND GUI_FORMS_SYSTEM_PYTHON3 AND TARGET gui_forms_gallery_model)
        add_executable(gui_forms_linux_accessibility_gallery_fixture
            tests/linux_accessibility_gallery_fixture.cpp
            ${GUI_FORMS_SOURCE_DIR}/src/surface/linux/accessibility/linux_accessibility.cpp
            ${GUI_FORMS_SOURCE_DIR}/src/surface/linux/services/x11_foreign_window.cpp)
        target_include_directories(gui_forms_linux_accessibility_gallery_fixture PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(gui_forms_linux_accessibility_gallery_fixture PRIVATE
            gui_forms_gallery_model gui_forms_host_headless PkgConfig::GUI_FORMS_TEST_ATK X11::X11)
        add_test(NAME gui_forms_linux_accessibility_gallery_tests
                 COMMAND ${GUI_FORMS_SYSTEM_PYTHON3} -B
                         ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux_accessibility_gallery_client.py
                         $<TARGET_FILE:gui_forms_linux_accessibility_gallery_fixture>
                         ${GUI_FORMS_SOURCE_DIR}/manifests/controls.json)
        # Gallery-wide: walks every page; `ctest -LE gallery_accessibility` skips it.
        set_property(TEST gui_forms_linux_accessibility_gallery_tests APPEND PROPERTY LABELS gallery_accessibility)
        set_tests_properties(gui_forms_linux_accessibility_gallery_tests PROPERTIES
            SKIP_RETURN_CODE 77 TIMEOUT 900)
    endif()
endif()
