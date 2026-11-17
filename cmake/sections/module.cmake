# Only a cache built with GUI_FORMS_BUILD_MODULE has the module (gui_forms cmake/Module.cmake).
if(TARGET gui_forms_module)
    # The example consumer: `import gui_forms;` and a window of controls, run.
    add_executable(gui_forms_module_consumer ${GUI_FORMS_SOURCE_DIR}/examples/module_consumer/main.cpp)
    target_link_libraries(gui_forms_module_consumer PRIVATE gui_forms_module)
    set_target_properties(gui_forms_module_consumer PROPERTIES CXX_SCAN_FOR_MODULES ON)
    add_test(NAME gui_forms_module_consumer COMMAND gui_forms_module_consumer)
endif()
