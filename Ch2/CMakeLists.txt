# mlir_tablegen() 只能把产物写到 build 树，而这里要求生成到源码树的 output/ 目录，
# 所以自己写 add_custom_command。好处是依赖关系仍然由构建系统维护：
# 改了 src/ops.td，构建时会自动重新生成 output/ 下的四个 .inc。
set(TOY_TABLEGEN_EXE ${LLVM_TOOLS_BINARY_DIR}/mlir-tblgen)
set(TOY_TD ${CMAKE_CURRENT_SOURCE_DIR}/src/ops.td)
set(TOY_GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/output)

set(TOY_TABLEGEN_INCLUDES ${LLVM_INCLUDE_DIRS} ${MLIR_INCLUDE_DIRS})
list(TRANSFORM TOY_TABLEGEN_INCLUDES PREPEND -I)
list(APPEND TOY_TABLEGEN_INCLUDES -I${CMAKE_CURRENT_SOURCE_DIR}/src)

file(MAKE_DIRECTORY ${TOY_GEN_DIR})

function(toy_tablegen ofn generator)
    add_custom_command(
        OUTPUT ${TOY_GEN_DIR}/${ofn}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${TOY_GEN_DIR}
        COMMAND ${TOY_TABLEGEN_EXE} -${generator}
                ${TOY_TABLEGEN_INCLUDES}
                ${TOY_TD}
                --write-if-changed
                -o ${TOY_GEN_DIR}/${ofn}
        DEPENDS ${TOY_TD}
        COMMENT "Generating ${ofn}"
    )
    set(TOY_TABLEGEN_OUTPUTS ${TOY_TABLEGEN_OUTPUTS} ${TOY_GEN_DIR}/${ofn} PARENT_SCOPE)
endfunction()

toy_tablegen(ops.h.inc gen-op-decls)
toy_tablegen(ops.cpp.inc gen-op-defs)
toy_tablegen(dialect.h.inc gen-dialect-decls)
toy_tablegen(dialect.cpp.inc gen-dialect-defs)

add_custom_target(ToyCh2OpsIncGen DEPENDS ${TOY_TABLEGEN_OUTPUTS})

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

target_link_libraries(toyc-ch2 PRIVATE
    MLIRAnalysis
    MLIRFunctionInterfaces
    MLIRIR
    MLIRParser
    MLIRSideEffectInterfaces
    MLIRSupport
    MLIRTransforms
)
