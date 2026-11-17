if(NOT GUI_FORMS_REFERENCE_ARITHMETIC)
    message(WARNING "GUI_FORMS_BUILD_TESTS is ON without GUI_FORMS_REFERENCE_ARITHMETIC: the tests labelled "
                    "reference_arithmetic pin the reference arithmetic's bytes and may fail where the compiler "
                    "fuses multiply-adds (arm64); run the rest with ctest -LE reference_arithmetic. "
                    "${GUI_FORMS_SOURCE_DIR}/tools/ci_build.py configures with -DGUI_FORMS_REFERENCE_ARITHMETIC=ON.")
endif()
