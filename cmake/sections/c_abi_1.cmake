# The C ABI's verification set, run by `ctest -L c_abi`: every test above,
# the drawing C API's tests, and the C++ binding and shadow-frame tests its
# tables stand on. A folding checkpoint runs this
# label rather than a name pattern, so a new ABI test joins by being listed.
set(_gf_abi_label_candidates
    gui_drawing_c_api_c11_tests gui_drawing_raster_c_api_tests
    gui_forms_abi_exports_only_entry_points gui_forms_abi_system_imports_only
    gui_forms_abi_allocator_boundary
    gui_forms_windows_gdi_compat_shim_tests
    gui_forms_binding_tests gui_forms_binding_provider_tests gui_forms_binding_navigator_tests
    gui_forms_shadow_frames_tests
    gui_forms_abi_runtime_c11_tests gui_forms_abi_conformance gui_forms_abi_slot_coverage
    gui_forms_abi_schema_export gui_forms_abi_theme_trace gui_forms_abi_capability_matrix
    gui_forms_abi_schema_catalog_current gui_forms_abi_event_fields gui_forms_abi_custom_paint
    gui_forms_abi_event_catalog_current gui_forms_abi_layout_probe_c11
    gui_forms_abi_layout_probe_cpp gui_forms_abi_audit gui_forms_abi_layout_probe_current
    gui_forms_abi_audit_fixtures gui_forms_abi_exports gui_forms_abi_audit_exports_tests gui_forms_abi_msvc_consumer
    gui_forms_abi_bindings_current gui_forms_abi_csharp
    gui_forms_abi_cpp_wrappers_current gui_forms_abi_cpp_wrappers)
get_property(_gf_abi_directory_tests DIRECTORY PROPERTY TESTS)
foreach(_gf_abi_directory_test IN LISTS _gf_abi_directory_tests)
    if(_gf_abi_directory_test MATCHES "^gui_forms_abi_theme_request_")
        list(APPEND _gf_abi_label_candidates ${_gf_abi_directory_test})
    endif()
endforeach()
foreach(_gf_abi_label_test IN LISTS _gf_abi_label_candidates)
    if(TEST ${_gf_abi_label_test})
        set_property(TEST ${_gf_abi_label_test} APPEND PROPERTY LABELS c_abi)
    endif()
endforeach()
