cmake_minimum_required(VERSION 3.4.3)
project(confinlog)

#===============================================================================
# 1. LOAD LLVM CONFIGURATION
#===============================================================================
# Set this to a valid LLVM installation dir
set(CT_Clang_INSTALL_DIR "/home/llvm-10.0.0/build/" CACHE PATH "LLVM installation directory")

# Add the location of ClangConfig.cmake to CMake search paths (so that
# find_package can locate it)
list(APPEND CMAKE_PREFIX_PATH "${CT_Clang_INSTALL_DIR}/lib/cmake/clang/")

find_package(Clang REQUIRED CONFIG)

# HelloWorld includes headers from LLVM and Clang - update the include paths
# accordingly
include_directories(SYSTEM "${LLVM_INCLUDE_DIRS};${CLANG_INCLUDE_DIRS}")

#===============================================================================
# 2. LLVM-TUTOR BUILD CONFIGURATION
#===============================================================================
# Use the same C++ standard as LLVM does
set(CMAKE_CXX_STANDARD 14 CACHE STRING "")

# LLVM is normally built without RTTI. Be consistent with that.
if(NOT LLVM_ENABLE_RTTI)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

#===============================================================================
# 3. ADD THE TARGET
#===============================================================================
add_executable(confinlog ConfInLog.cpp  ConfInfo.cpp  LogInfo.cpp  WordNinja.cpp)

target_link_libraries(
  confinlog
  clangTooling
)

# Configure include directories for 'tool'
target_include_directories(
  confinlog
  PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../include"
)