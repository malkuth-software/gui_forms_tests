# Cut from gui_forms' CMakeLists.txt on 2026-11-22 (roadmap R1, the end of
# the test-programs move): stage F's metrics differential (tools/font_metrics_differential), its
# library and its program, which the font metrics tests link. Paths into gui_forms read the cache's
# source tree (GUI_FORMS_SOURCE_DIR); the programs' own sources are here.

# Stage F's metrics differential (tools/font_metrics_differential): the
# bundled engine's measurements of two faces, a GDI oracle on Windows, and
# the comparison under a mapping's tolerance.
add_library(gui_forms_font_metrics_differential STATIC
    tools/font_metrics_differential/differential_inputs.cpp
    tools/font_metrics_differential/engine_measurement.cpp
    tools/font_metrics_differential/differential_compare.cpp
    tools/font_metrics_differential/differential_report.cpp
    tools/font_metrics_differential/metric_snapshot.cpp
)
if(WIN32)
    target_sources(gui_forms_font_metrics_differential PRIVATE
        tools/font_metrics_differential/gdi_measurement_windows.cpp)
    target_link_libraries(gui_forms_font_metrics_differential PRIVATE gdi32)
else()
    target_sources(gui_forms_font_metrics_differential PRIVATE
        tools/font_metrics_differential/gdi_measurement_unavailable.cpp)
endif()
target_include_directories(gui_forms_font_metrics_differential
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tools/font_metrics_differential)
target_link_libraries(gui_forms_font_metrics_differential
    PUBLIC gui_forms_core
    PRIVATE gui_forms_text_engine stxlib::algorithms)
add_executable(gui_forms_font_metrics_differential_tool
    tools/font_metrics_differential/font_metrics_differential_main.cpp)
# The program's import library (a Windows link writes one beside each
# executable, named after its output) keeps the target's own name, so that it
# never overwrites the library of the same output name.
set_target_properties(gui_forms_font_metrics_differential_tool PROPERTIES
    OUTPUT_NAME gui_forms_font_metrics_differential
    ARCHIVE_OUTPUT_NAME gui_forms_font_metrics_differential_tool)
target_link_libraries(gui_forms_font_metrics_differential_tool PRIVATE
    gui_forms_font_metrics_differential)
