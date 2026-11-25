# Cut from gui_forms' CMakeLists.txt on 2026-11-22 (roadmap R1, the end of
# the test-programs move, with the profiling and present workloads in its
# second pass): the measurement harnesses, workloads and probes
# (benchmarks/), none of them a CTest entry. Paths into gui_forms read the
# cache's source tree (GUI_FORMS_SOURCE_DIR); the programs' own sources are
# here.

option(GUI_FORMS_BUILD_BENCHMARKS "Build the renderer benchmark and its boundary audit (macOS, Skia, the Gallery model)" OFF)
option(GUI_FORMS_BUILD_TEXT_LAYOUT_PROBE "Build the private text shaping diagnostic" OFF)
option(GUI_FORMS_BUILD_LABEL_LAYOUT_PROBE "Build the fixed Label measurement diagnostic" OFF)
option(GUI_FORMS_BUILD_TEXT_WORKER_PROBE "Build the private text worker lifecycle proof" OFF)
option(GUI_FORMS_BUILD_TEXT_PAINT_PROBE "Build the private prepared glyph raster proof" OFF)
option(GUI_FORMS_BUILD_DOCUMENT_VIEW_BENCHMARK "Build the renderer-neutral D1 measurement harness" OFF)
option(GUI_FORMS_BUILD_TEXT_ENGINE_BENCHMARK "Build the shared text engine cost benchmark" OFF)
option(GUI_FORMS_BUILD_PAINT_PROFILE "Build the Gallery paint workload that the Linux profiling job samples" OFF)
option(GUI_FORMS_BUILD_BLEND_STUDY "Build the Gallery blend and opaque study workload (tools/blend_study.py)" OFF)

if(GUI_FORMS_BUILD_TEXT_LAYOUT_PROBE)
    add_executable(gui_forms_text_layout_probe benchmarks/text_layout_probe.cpp)
    target_link_libraries(gui_forms_text_layout_probe PRIVATE gui_forms_text_engine)
endif()

if(GUI_FORMS_BUILD_LABEL_LAYOUT_PROBE)
    add_executable(gui_forms_label_layout_probe benchmarks/label_layout_probe.cpp)
    target_link_libraries(gui_forms_label_layout_probe PRIVATE gui_forms_controls gui_forms_text_engine)
endif()

if(GUI_FORMS_BUILD_TEXT_WORKER_PROBE)
    find_package(Threads REQUIRED)
    add_executable(gui_forms_text_worker_probe
        benchmarks/text_worker_probe.cpp
        benchmarks/text_probes/worker_probe.cpp)
    target_include_directories(gui_forms_text_worker_probe PRIVATE benchmarks/text_probes)
    target_link_libraries(gui_forms_text_worker_probe PRIVATE gui_forms_text_engine Threads::Threads)
endif()

if(GUI_FORMS_BUILD_TEXT_PAINT_PROBE)
    find_package(Threads REQUIRED)
    add_executable(gui_forms_text_paint_probe
        benchmarks/text_paint_probe.cpp
        benchmarks/text_probes/paint_probe.cpp
        benchmarks/text_probes/worker_probe.cpp)
    target_include_directories(gui_forms_text_paint_probe PRIVATE
        benchmarks/text_probes)
    target_link_libraries(gui_forms_text_paint_probe PRIVATE gui_forms_text_engine stxlib::algorithms Threads::Threads)
endif()

# Stage C5: the cost of shadow frames, alone and over a window workload.
# Built only when asked for.
add_executable(gui_forms_shadow_frames_benchmark EXCLUDE_FROM_ALL benchmarks/shadow_frames_benchmark.cpp)
target_link_libraries(gui_forms_shadow_frames_benchmark PRIVATE gui_forms_core)

# IMGUI_IMPLOT_STUDY B.7: the plots package's costs (pyramid build, pan and
# live frames, scatter thinning at a million points). Built only when asked
# for; run in Release.
add_executable(gui_forms_plot_performance_probe EXCLUDE_FROM_ALL benchmarks/plot_performance_probe.cpp)
target_link_libraries(gui_forms_plot_performance_probe PRIVATE gui_forms_plot_primitives)

# Audit addendum batch d: the per-operation cost of a window of several
# thousand controls (attach, pointer, focus, layout, paint, inherited reads,
# bulk clear). Built only when asked for; run in Release.
add_executable(gui_forms_tree_cost_benchmark EXCLUDE_FROM_ALL benchmarks/tree_cost_benchmark.cpp)
target_link_libraries(gui_forms_tree_cost_benchmark PRIVATE gui_forms_core)

# APP_WINDOW_RULES_DESIGN K.2: the creation-cost gate (a bare window, a popup
# and a generated form window; time and allocations against the committed
# baseline benchmarks/creation/creation_baseline.txt). Built only when asked
# for; run in Release on the M4.
if(TARGET gui_forms_form_compiler AND GUI_FORMS_BUILD_GENERATED_FORM_TESTS)
    add_executable(gui_forms_creation_benchmark EXCLUDE_FROM_ALL benchmarks/creation_benchmark.cpp)
    target_link_libraries(gui_forms_creation_benchmark PRIVATE gui_forms_controls gui_forms_host_headless)
    target_include_directories(gui_forms_creation_benchmark PRIVATE ${GUI_FORMS_SOURCE_DIR}/src/host/headless)
    gui_forms_add_form(gui_forms_creation_benchmark benchmarks/creation/creation_form.form
        CLASS CreationSettingsForm NAMESPACE form_generated)
endif()

if(GUI_FORMS_BUILD_BENCHMARKS)
    if(NOT APPLE OR NOT TARGET gui_forms_skia OR NOT TARGET gui_forms_gallery_model)
        message(FATAL_ERROR
            "The renderer benchmark requires macOS, Skia, and the Gallery model")
    endif()
    add_executable(gui_forms_renderer_benchmark
        benchmarks/renderer_benchmark.cpp
    )
    target_compile_definitions(gui_forms_renderer_benchmark PRIVATE
        GUI_FORMS_BENCH_REGULAR_FONT="${GUI_FORMS_SOURCE_DIR}/assets/fonts/PortsmouthRapids.ttf"
        GUI_FORMS_BENCH_BOLD_FONT="${GUI_FORMS_SOURCE_DIR}/assets/fonts/PortsmouthRapids-Bold.ttf"
    )
    target_link_libraries(gui_forms_renderer_benchmark PRIVATE
        gui_forms_gallery_model
        gui_forms_skia
    )
    add_custom_target(gui_forms_renderer_boundary_audit
        COMMAND ${CMAKE_COMMAND}
            -DCORE_LIBRARY=$<TARGET_FILE:gui_forms_core>
            -DSKIA_ADAPTER=$<TARGET_FILE:gui_forms_skia>
            -DBENCHMARK_BINARY=$<TARGET_FILE:gui_forms_renderer_benchmark>
            -DPUBLIC_INCLUDE_DIRECTORY=${GUI_FORMS_SOURCE_DIR}/include
            -DSKIA_ARCHIVE=$<TARGET_FILE:stx_skia::stx_skia>
            -DOUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/renderer-boundary-audit.txt
            -P ${GUI_FORMS_SOURCE_DIR}/cmake/check_renderer_boundaries.cmake
        DEPENDS gui_forms_renderer_benchmark
        VERBATIM
    )
endif()

# The focus labs (claude/perf-focus-labs, benchmarks/focus_labs): measurement programs,
# not tests, so no CTest entry. Each checks its rearranged kernels against the library's
# own for equal output, then times both. composite_lab compiles the compositor's own
# translation unit; bake_lab links as gui_forms_engine_material_tests does, exec_lab as
# gui_drawing_skia_tests does. Since the perf fold (cp29) the library carries the
# rearrangements, so the ratios measure what is left.
option(GUI_FORMS_BUILD_FOCUS_LABS "Build the benchmarks/focus_labs programs" OFF)
if(GUI_FORMS_BUILD_FOCUS_LABS)
    add_executable(focus_composite_lab benchmarks/focus_labs/composite_lab.cpp
        ${GUI_FORMS_SOURCE_DIR}/src/core/text/composite/text_composite.cpp)
    target_include_directories(focus_composite_lab PRIVATE ${GUI_FORMS_SOURCE_DIR}/include
        ${GUI_FORMS_SOURCE_DIR}/src ${GUI_FORMS_SOURCE_DIR}/src/core/text/composite)
    target_compile_features(focus_composite_lab PRIVATE cxx_std_20)
    add_executable(focus_bake_lab benchmarks/focus_labs/bake_lab.cpp)
    target_link_libraries(focus_bake_lab PRIVATE gui_forms_controls)
    if(TARGET gui_drawing_skia)
        add_executable(focus_exec_lab benchmarks/focus_labs/exec_lab.cpp
            $<TARGET_OBJECTS:gui_forms_sealed_pages_objects>)
        target_link_libraries(focus_exec_lab PRIVATE gui_drawing_skia gui_forms_core)
        target_include_directories(focus_exec_lab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    endif()
endif()

if(GUI_FORMS_BUILD_TEXT_ENGINE_BENCHMARK)
    add_executable(gui_forms_text_engine_benchmark benchmarks/text_engine_benchmark.cpp)
    target_link_libraries(gui_forms_text_engine_benchmark PRIVATE gui_forms_text_engine gui_forms_core)
endif()

if(GUI_FORMS_BUILD_DOCUMENT_VIEW_BENCHMARK)
    add_executable(gui_forms_document_view_benchmark benchmarks/document_view_benchmark.cpp)
    target_link_libraries(gui_forms_document_view_benchmark PRIVATE gui_forms_core)
endif()

# Phase E, E5: the Gallery painted through the CPU Skia adapter, for
# .github/workflows/profile-linux.yml to sample (tools/profile_hot_paths.py,
# against a profile test cache: gui_forms tools/profile_cache.py).
if(GUI_FORMS_BUILD_PAINT_PROFILE)
    if(NOT TARGET gui_forms_skia OR NOT TARGET gui_forms_gallery_model)
        message(FATAL_ERROR "The paint profile workload requires a test cache with Skia and the Gallery model")
    endif()
    add_executable(gui_forms_paint_profile_workload benchmarks/paint_profile_workload.cpp)
    target_compile_definitions(gui_forms_paint_profile_workload PRIVATE
        GUI_FORMS_PROFILE_FONT_DIRECTORY="${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    target_link_libraries(gui_forms_paint_profile_workload PRIVATE gui_forms_gallery_model gui_forms_skia)
endif()

# The blend and opaque study (2026-11-16): the Gallery's blend counters and
# frame times through the CPU Skia adapter, for tools/blend_study.py.
if(GUI_FORMS_BUILD_BLEND_STUDY)
    if(NOT TARGET gui_forms_skia OR NOT TARGET gui_forms_gallery_model)
        message(FATAL_ERROR "The blend study workload requires a test cache with Skia and the Gallery model")
    endif()
    add_executable(gui_forms_blend_study_workload benchmarks/blend_study_workload.cpp)
    target_compile_definitions(gui_forms_blend_study_workload PRIVATE
        GUI_FORMS_BLEND_STUDY_FONT_DIRECTORY="${GUI_FORMS_SOURCE_DIR}/assets/fonts")
    target_link_libraries(gui_forms_blend_study_workload PRIVATE gui_forms_gallery_model gui_forms_skia)
endif()


# SK2: the time each host spends presenting a frame, which the tests job
# measures after the suite (.github/workflows/tests.yml). Built only when
# asked for.
if(TARGET gui_forms_application)
    add_executable(gui_forms_present_profile_workload EXCLUDE_FROM_ALL benchmarks/present_profile_workload.cpp)
    target_link_libraries(gui_forms_present_profile_workload PRIVATE gui_forms_application)
endif()
