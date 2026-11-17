# The test programs that compile windows_host.cpp, and the ones after it by including it
# textually, link stx_skia::swizzle as the Windows host does (gui_forms CMakeLists.txt).
foreach(gui_forms_swizzle_target IN ITEMS
        gui_forms_windows_dib_lifecycle_tests
        gui_forms_windows_prepared_text_tests gui_forms_windows_backplane_replay_tests
        gui_forms_windows_canvas_raster_tests gui_forms_windows_content_pixels_tests
        gui_forms_windows_control_snapshot_tests gui_forms_windows_damage_rectangles_tests
        gui_forms_windows_scroll_copy_tests gui_forms_windows_live_idle_tests
        gui_forms_windows_schedule_wake_tests gui_forms_windows_message_box_tests gui_forms_windows_text_tests)
    if(TARGET ${gui_forms_swizzle_target})
        target_link_libraries(${gui_forms_swizzle_target} PRIVATE stx_skia::swizzle)
    endif()
endforeach()
