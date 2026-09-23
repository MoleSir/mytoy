set(LLVM_TARGET_DEFINITIONS src/ops.td)
mlir_tablegen(ops.h.inc -gen-op-decls)
mlir_tablegen(ops.cpp.inc -gen-op-defs)
mlir_tablegen(dialect.h.inc -gen-dialect-decls)
mlir_tablegen(dialect.cpp.inc -gen-dialect-defs)
add_public_tablegen_target(ToyCh2OpsIncGen)

set(LLVM_LINK_COMPONENTS Support)

add_toy_chapter(
    toyc-ch2 
    main.cc 
    src/ast.cc
    src/dialect.cc
    src/mlir_gen.cc
    DEPENDS ToyCh2OpsIncGen
)

include_directories(src)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(toyc-ch2 PRIVATE
    MLIRAnalysis
    MLIRFunctionInterfaces
    MLIRIR
    MLIRParser
    MLIRSideEffectInterfaces
    MLIRSupport
    MLIRTransforms
)
