if(GUI_FORMS_BUILD_AUDIO)
    if(TARGET stx_vorbis::stx_vorbis_stb)
        # stx_vorbis's decoder over a mutated stream (audit I3).
        gui_forms_add_fuzz_test(vorbis SOURCE ${GUI_FORMS_SOURCE_DIR}/fuzz/vorbis_fuzz.cpp LIBRARIES stx_vorbis::stx_vorbis_stb
            ARGUMENTS ${GUI_FORMS_SOURCE_DIR}/examples/reference/tone.ogg)
    endif()
endif()
