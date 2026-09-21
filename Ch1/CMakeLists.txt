set(LLVM_LINK_COMPONENTS Support)
add_toy_chapter(toyc-ch1 main.cc src/ast.cc)
include_directories(src)
target_link_libraries(toyc-ch1 PRIVATE MLIRSupport)
