cmake_minimum_required(VERSION 3.20.0)
project(toy-mlir-tutorial LANGUAGES CXX C)

set(CMAKE_BUILD_WITH_INSTALL_NAME_DIR ON)
set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard to conform to")

# Out-of-tree build against a prebuilt LLVM/MLIR tree. Point MLIR_DIR at either
# <llvm-build>/lib/cmake/mlir (no `ninja install` needed) or <prefix>/lib/cmake/mlir.
find_package(MLIR REQUIRED CONFIG)

message(STATUS "Using MLIRConfig.cmake in: ${MLIR_DIR}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/bin)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/lib)
set(MLIR_BINARY_DIR ${CMAKE_BINARY_DIR})

list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")

include(TableGen)
include(AddLLVM)
include(AddMLIR)
include(HandleLLVMOptions)

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${MLIR_INCLUDE_DIRS})
link_directories(${LLVM_BUILD_LIBRARY_DIR})
add_definitions(${LLVM_DEFINITIONS})

set(LLVM_LIT_ARGS "-sv" CACHE STRING "Default options for lit")

# `configure_lit_site_cfg` and `add_lit_testsuite` need the lit driver; default
# it to the host build's llvm-lit so `check-toy` works out of the box.
if(NOT LLVM_EXTERNAL_LIT)
  find_program(LLVM_EXTERNAL_LIT llvm-lit
    HINTS "${LLVM_TOOLS_BINARY_DIR}" "${MLIR_DIR}/../../bin" NO_DEFAULT_PATH)
  if(NOT LLVM_EXTERNAL_LIT)
    message(WARNING "llvm-lit not found; pass -DLLVM_EXTERNAL_LIT=<path> to "
                    "enable the check-toy target")
  endif()
endif()

set(TOY_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(TOY_BINARY_DIR ${CMAKE_BINARY_DIR})
set(LLVM_TOOLS_DIR ${LLVM_TOOLS_BINARY_DIR})

option(MLIR_INCLUDE_TESTS "Build the Toy tutorial lit tests" ON)

# The chapter CMakeLists.txt files are copied verbatim from mlir/examples/toy.
# In-tree they call `add_toy_chapter`, defined in mlir/examples/toy/CMakeLists.txt
# on top of the in-tree `add_llvm_example`. Recreate it here so that the chapter
# files themselves need no edits. Unlike add_llvm_example this does not set
# EXCLUDE_FROM_ALL, so a bare `ninja` builds every chapter.
macro(add_toy_chapter name)
  add_llvm_executable(${name} ${ARGN})
endmacro(add_toy_chapter name)

add_subdirectory(Ch1)
add_subdirectory(Ch2)
add_subdirectory(Ch3)
add_subdirectory(Ch4)
add_subdirectory(Ch5)
add_subdirectory(Ch6)
add_subdirectory(Ch7)

if(MLIR_INCLUDE_TESTS)
  add_subdirectory(test)
endif()
