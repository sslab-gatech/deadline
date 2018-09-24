cmake_minimum_required(VERSION 3.5.1)

find_package(LLVM REQUIRED CONFIG)

# llvm deps
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

# util dependencies
include_directories("${DEPS_UTIL_DIR}")

# z3
include_directories("${DEPS_Z3_DIR}/bins/include")
link_directories("${DEPS_Z3_DIR}/bins/lib")
link_libraries(z3)

# flags
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS_RELEASE 
  "${CMAKE_CXX_FLAGS}")

set(CMAKE_CXX_FLAGS_PROFILE 
  "${CMAKE_CXX_FLAGS} -DKSYM_DEBUG -DKSYM_DEBUG_${KSYM_DEBUG_ITEM}")

# ksym
add_subdirectory(KSym)
