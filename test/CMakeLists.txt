configure_lit_site_cfg(
        ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
        ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py
        MAIN_CONFIG
        ${CMAKE_CURRENT_SOURCE_DIR}/lit.cfg.py
        )

set(TOY_TEST_DEPENDS
        toyc-ch1
        toyc-ch2
        toyc-ch3
        toyc-ch4
        toyc-ch5
        toyc-ch6
        toyc-ch7
        )

add_lit_testsuite(check-toy "Running the MLIR Toy tutorial regression tests"
        ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS ${TOY_TEST_DEPENDS}
        )
set_target_properties(check-toy PROPERTIES FOLDER "Tests")
