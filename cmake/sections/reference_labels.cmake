# The byte-exact tests: pinned pixel hashes and cross-host comparisons of code
# whose arithmetic follows GUI_FORMS_REFERENCE_ARITHMETIC. They pass only in
# a reference build; a deployment build (the release programs job, which
# builds what ships with the option OFF) runs the suite with
# `ctest -LE reference_arithmetic`, and the reference jobs (native, sanitizer,
# the engine gate) run them all.
foreach(gui_forms_reference_test IN ITEMS
        gui_forms_engine_gate_tests
        gui_forms_engine_fill_shape_tests
        gui_forms_engine_filter_blend_tests
        gui_forms_engine_material_tests
        gui_forms_engine_reflection_tests
        gui_forms_engine_dither_tests
        gui_forms_engine_relief_tests
        gui_forms_engine_finish_tests
        gui_forms_engine_primitive_tests
        gui_forms_engine_procedural_tests
        gui_forms_engine_light_tests
        gui_forms_engine_motion_tests
        gui_forms_engine_glass_tests
        gui_forms_engine_backdrop_tests
        gui_forms_engine_backplane_replay_tests
        gui_forms_backplane_replay_identity_tests
        gui_forms_windows_backplane_replay_tests
        gui_forms_part_composite_identity_tests
        gui_forms_text_identity_raster_tests
        gui_forms_windows_text_tests
        gui_forms_png_color_tests
        gui_forms_png_pixels_tests
        gui_forms_png_writer_tests
        gui_forms_plot_primitives_tests)
    if(TEST ${gui_forms_reference_test})
        set_property(TEST ${gui_forms_reference_test} APPEND PROPERTY LABELS reference_arithmetic)
    endif()
endforeach()
