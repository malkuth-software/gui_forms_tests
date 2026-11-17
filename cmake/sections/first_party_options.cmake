include(${GUI_FORMS_SOURCE_DIR}/cmake/Hardening.cmake)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    get_property(gui_forms_first_party_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(gui_forms_first_party_target IN LISTS gui_forms_first_party_targets)
        get_target_property(gui_forms_target_type ${gui_forms_first_party_target} TYPE)
        if(gui_forms_target_type MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY|EXECUTABLE)$")
            if(GUI_FORMS_REFERENCE_ARITHMETIC)
                target_compile_options(${gui_forms_first_party_target} PRIVATE
                    $<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-ffp-contract=off>)
                target_compile_definitions(${gui_forms_first_party_target} PRIVATE
                    GUI_FORMS_REFERENCE_ARITHMETIC=1)
            endif()
            # Hardening and the warning level (cmake/Hardening.cmake, D11).
            gui_forms_harden_target(${gui_forms_first_party_target} ${gui_forms_target_type})
            # A view (std::string_view, std::span) bound or assigned to a
            # temporary dangles: PartPaint::text twice held a temporary
            # std::string (CorrespondenceView, PropertyList) and painting read
            # freed memory. Clang reports these on by default; here they fail
            # the build. A value assigned to itself is never meant: a property
            # forwarder whose parameter shadowed its cell
            # (`set_visible(bool visible) { visible = visible; }`) stored
            # nothing and the Gallery hid nothing. -Wself-assign is off by
            # default; here it fails the build.
            if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
                target_compile_options(${gui_forms_first_party_target} PRIVATE
                    $<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Werror=dangling-assignment-gsl -Werror=dangling-gsl
                        -Werror=self-assign -Werror=self-assign-field>)
            endif()
        endif()
    endforeach()
    # Contraction off in every build, whatever GUI_FORMS_REFERENCE_ARITHMETIC
    # says (each source also carries the Clang pragma; GCC reads only the flag):
    # - engine_math: its stated error bounds (engine_math.hpp) assume each
    #   operation rounds on its own;
    # - engine_expression and theme_value: theme values and expressions, the
    #   same text giving the same number on every host;
    # - value_text: a ValueBar's typed value, parsed as the expression engine
    #   computes;
    # - scrub_gesture: a pointer script's scrubbed values;
    # - layout_style_reader: layout lengths, which place edges and hit tests;
    # - theme_motion_lowering and motion_value: motion parameters and samples,
    #   whose settling decides when an animation ends.
    set_source_files_properties(
        ${GUI_FORMS_SOURCE_DIR}/src/core/engine/engine_math/engine_math.cpp
        ${GUI_FORMS_SOURCE_DIR}/src/core/engine/engine_expression/engine_expression.cpp
        ${GUI_FORMS_SOURCE_DIR}/src/core/theme_language/value/theme_value.cpp
        ${GUI_FORMS_SOURCE_DIR}/src/controls/range_control/value_bar/value_text.cpp
        ${GUI_FORMS_SOURCE_DIR}/src/core/drag_engine/scrub_gesture.cpp
        ${GUI_FORMS_SOURCE_DIR}/src/core/layout/style_reader/layout_style_reader.cpp
        ${GUI_FORMS_SOURCE_DIR}/src/core/theme_language/motion/theme_motion_lowering.cpp
        ${GUI_FORMS_SOURCE_DIR}/src/core/engine/motion_value/motion_value.cpp
        PROPERTIES COMPILE_OPTIONS -ffp-contract=off)
endif()
