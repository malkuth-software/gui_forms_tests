# The SDK-consumer link test: the tree installed into a temporary prefix, and
# tests/sdk_consumer built against that prefix alone, using every public
# component (the controls, a composite, an application font family, the
# drawing raster seam, the text masks with the prepared-text vocabulary, and
# the C ABI with gd_get_api_v0). A symbol the installed archives lack fails
# its link; the one-module SDK once shipped an empty TextMasks interface and
# an Application archive without the text masks (b68c80ecc, 2d4bb797b). The
# consumer is configured as this tree was, so the same compiler and flags
# link the archives. Only an installable tree registers it: a native host
# (Application), and no development prepared-text interface, whose install is
# refused. tools/verify_install.py --sdk-consumer runs the same consumer
# against a prefix built elsewhere.
if(TARGET gui_forms_application AND NOT GUI_FORMS_BUILD_PREPARED_TEXT)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(gui_forms_sdk_consumer_text_masks OFF)
        if(TARGET gui_forms_text_masks)
            set(gui_forms_sdk_consumer_text_masks ON)
        endif()
        set(gui_forms_sdk_consumer_c_abi OFF)
        if(TARGET gui_forms_abi)
            set(gui_forms_sdk_consumer_c_abi ON)
        endif()
        set(gui_forms_sdk_consumer_cmake_arguments
            "-G${CMAKE_GENERATOR}"
            "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
            "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
            "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
            "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
            "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
            "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
            "-DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
            "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
            "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}"
            "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
        add_test(NAME gui_forms_sdk_consumer_link
            COMMAND ${Python3_EXECUTABLE} -B ${GUI_FORMS_SOURCE_DIR}/tools/sdk_consumer_link.py
                --prefix ${GUI_FORMS_TEST_CACHE_PREFIX}
                --scratch ${CMAKE_CURRENT_BINARY_DIR}/sdk-consumer-link
                --config $<CONFIG>
                --text-masks ${gui_forms_sdk_consumer_text_masks}
                --c-abi ${gui_forms_sdk_consumer_c_abi}
                -- ${gui_forms_sdk_consumer_cmake_arguments})
        # It installs the whole tree and builds a program: minutes, not seconds.
        # A tree without the text masks or the C ABI library proves less:
        # the tool says so and exits 77, which CTest reports as skipped
        # instead of passed (CI's tree builds the masks, tools/ci_build.py).
        set_tests_properties(gui_forms_sdk_consumer_link PROPERTIES TIMEOUT 1200 LABELS sdk SKIP_RETURN_CODE 77)
    endif()
endif()
