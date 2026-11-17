if(GUI_FORMS_FONTPACK_TOOL)
    set(_gf_fontpack_tool_file ${GUI_FORMS_FONTPACK_TOOL})
else()
    set(_gf_fontpack_tool_file $<TARGET_FILE:stx_fontpack_tool>)
endif()
add_test(NAME gui_forms_font_pack_round_trip
         COMMAND ${_gf_fontpack_tool_file} verify ${GUI_FORMS_FONT_PACK} ${GUI_FORMS_FONT_PACK_FACES})
