# Cut from gui_forms' CMakeLists.txt on 2026-11-22 (roadmap R1, the end of
# the test-programs move): the libFuzzer programs (GUI_FORMS_BUILD_FUZZERS; the
# suite's standalone fuzz tests are in main.cmake and audio_fuzz.cmake). Paths into gui_forms read the cache's
# source tree (GUI_FORMS_SOURCE_DIR); the programs' own sources are here.

option(GUI_FORMS_BUILD_FUZZERS "Build libFuzzer entry points" OFF)
set(GUI_FORMS_STX_SOURCE_DIR "${GUI_FORMS_STX_SOURCE_DIR}" CACHE PATH "An stx checkout, for the codec fuzzers")

if(GUI_FORMS_BUILD_FUZZERS)
    # The codec fuzzers compile stx sources, so they need an stx checkout
    # (the commit gui_forms' stx.lock pins).
    if(GUI_FORMS_STX_SOURCE_DIR STREQUAL "")
        message(FATAL_ERROR "GUI_FORMS_BUILD_FUZZERS needs GUI_FORMS_STX_SOURCE_DIR, an stx checkout at the "
                            "commit ${GUI_FORMS_SOURCE_DIR}/stx.lock pins")
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "GUI_FORMS_BUILD_FUZZERS currently requires Clang libFuzzer")
    endif()
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=fuzzer")
    check_cxx_source_compiles(
        "#include <cstddef>\nextern \"C\" int LLVMFuzzerTestOneInput(const unsigned char*, std::size_t) { return 0; }"
        GUI_FORMS_HAS_LIBFUZZER_RUNTIME
    )
    unset(CMAKE_REQUIRED_FLAGS)
    # The inflate and PNG decode harnesses are stx's (stx_zlib/fuzz, stx_png/fuzz).
    set(stx_zlib_dir ${GUI_FORMS_STX_SOURCE_DIR}/stx_zlib)
    set(stx_png_dir ${GUI_FORMS_STX_SOURCE_DIR}/stx_png)
    gui_forms_add_libfuzzer_target(inflate
        SOURCES ${stx_zlib_dir}/fuzz/inflate_fuzz.cpp ${stx_zlib_dir}/src/inflate.cpp ${stx_zlib_dir}/src/checksum.cpp
        INCLUDES ${stx_zlib_dir}/include
        STANDALONE_DEFINITION STX_ZLIB_FUZZ_STANDALONE)
    gui_forms_add_libfuzzer_target(inflate_protocol
        SOURCES fuzz/inflate_protocol_fuzz.cpp ${stx_zlib_dir}/src/inflate.cpp ${stx_zlib_dir}/src/checksum.cpp
        INCLUDES ${GUI_FORMS_SOURCE_DIR}/include ${stx_zlib_dir}/include)
    gui_forms_add_libfuzzer_target(deflate
        SOURCES fuzz/deflate_fuzz.cpp ${stx_zlib_dir}/src/inflate.cpp ${stx_zlib_dir}/src/deflate.cpp
                ${stx_zlib_dir}/src/checksum.cpp
        INCLUDES ${stx_zlib_dir}/include)
    gui_forms_add_libfuzzer_target(png_decode
        SOURCES ${stx_png_dir}/fuzz/png_decode_fuzz.cpp ${stx_png_dir}/src/decoder.cpp ${stx_png_dir}/src/encoder.cpp
                ${stx_zlib_dir}/src/inflate.cpp ${stx_zlib_dir}/src/deflate.cpp ${stx_zlib_dir}/src/checksum.cpp
        INCLUDES ${stx_png_dir}/include ${stx_zlib_dir}/include
        STANDALONE_DEFINITION STX_PNG_FUZZ_STANDALONE)
    gui_forms_add_libfuzzer_target(png_registry
        SOURCES fuzz/png_registry_fuzz.cpp ${GUI_FORMS_SOURCE_DIR}/src/core/resources/image_registry/image_registry.cpp
                ${stx_zlib_dir}/src/checksum.cpp
        INCLUDES ${GUI_FORMS_SOURCE_DIR}/include ${stx_zlib_dir}/include)

    # The theme sheet tokenizer, parser and value reader (stage T3), from
    # their own sources.
    gui_forms_add_libfuzzer_target(theme_sheet
        SOURCES
            fuzz/theme_sheet_fuzz.cpp
            ${GUI_FORMS_SOURCE_DIR}/src/core/theme_language/source_text/theme_source_text.cpp
            ${GUI_FORMS_SOURCE_DIR}/src/core/theme_language/diagnostics/theme_diagnostics.cpp
            ${GUI_FORMS_SOURCE_DIR}/src/core/theme_language/number/theme_number.cpp
            ${GUI_FORMS_SOURCE_DIR}/src/core/theme_language/tokenizer/theme_tokenizer.cpp
            ${GUI_FORMS_SOURCE_DIR}/src/core/theme_language/syntax_tree/theme_syntax_tree.cpp
            ${GUI_FORMS_SOURCE_DIR}/src/core/theme_language/syntax_tree/theme_sheet_parser.cpp
            ${GUI_FORMS_SOURCE_DIR}/src/core/theme_language/custom_properties/theme_custom_properties.cpp
            ${GUI_FORMS_SOURCE_DIR}/src/core/theme_language/value/theme_value.cpp
            ${GUI_FORMS_SOURCE_DIR}/src/core/theme_language/media/theme_media_query.cpp
            ${GUI_FORMS_SOURCE_DIR}/src/core/engine/engine_expression/engine_expression.cpp
            ${GUI_FORMS_SOURCE_DIR}/src/core/engine/theme_rules/colour_roles.cpp
            ${GUI_FORMS_SOURCE_DIR}/src/core/anatomy/types/anatomy_types.cpp
            ${GUI_FORMS_SOURCE_DIR}/src/core/schema/types/schema_types.cpp
            ${GUI_FORMS_SOURCE_DIR}/src/core/xml/xml_characters/xml_characters.cpp
            ${GUI_FORMS_SOURCE_DIR}/src/core/xml/types/xml_types.cpp
        INCLUDES ${GUI_FORMS_SOURCE_DIR}/include)

    # The layout XML reader (DK decision 43), built from its own sources so
    # the sanitizers cover exactly the reader and writer.
    gui_forms_add_libfuzzer_target(xml_reader
        SOURCES fuzz/xml_reader_fuzz.cpp ${GUI_FORMS_SOURCE_DIR}/src/core/xml/types/xml_types.cpp ${GUI_FORMS_SOURCE_DIR}/src/core/xml/xml_characters/xml_characters.cpp
                ${GUI_FORMS_SOURCE_DIR}/src/core/xml/xml_reader/xml_reader.cpp ${GUI_FORMS_SOURCE_DIR}/src/core/xml/xml_writer/xml_writer.cpp
        INCLUDES ${GUI_FORMS_SOURCE_DIR}/include)

    # The theme manifest's JSON reader (stage T3, P10), from its own source.
    gui_forms_add_libfuzzer_target(json_reader
        SOURCES fuzz/json_reader_fuzz.cpp ${GUI_FORMS_SOURCE_DIR}/src/core/json/json_reader/json_reader.cpp
        INCLUDES ${GUI_FORMS_SOURCE_DIR}/include)

    # Whole subsystems, linked rather than rebuilt (audit N115, I3): the
    # sanitizers reach into the libraries only when the tree is configured
    # with them (cmake/FuzzTargets.cmake).
    gui_forms_add_libfuzzer_target(theme_compile SOURCES fuzz/theme_compile_fuzz.cpp LIBRARIES gui_forms_controls)
    gui_forms_add_libfuzzer_target(theme_manifest SOURCES fuzz/theme_manifest_fuzz.cpp LIBRARIES gui_forms_core)
    gui_forms_add_libfuzzer_target(form_document SOURCES fuzz/form_document_fuzz.cpp LIBRARIES gui_forms_controls)
    gui_forms_add_libfuzzer_target(dock_layout SOURCES fuzz/dock_layout_fuzz.cpp LIBRARIES gui_forms_core)
    gui_forms_add_libfuzzer_target(clipboard_image SOURCES fuzz/clipboard_image_fuzz.cpp LIBRARIES gui_forms_core)
    gui_forms_add_libfuzzer_target(rtf_import SOURCES fuzz/rtf_import_fuzz.cpp
        LIBRARIES gui_forms_core stx_rtf2md::stx_rtf2md)
    if(TARGET stx_vorbis::stx_vorbis_stb)
        gui_forms_add_libfuzzer_target(vorbis SOURCES fuzz/vorbis_fuzz.cpp LIBRARIES stx_vorbis::stx_vorbis_stb)
    endif()
endif()
