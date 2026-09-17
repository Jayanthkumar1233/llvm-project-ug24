# CMake toolchain file for the uG24 microprocessor (ug24-unknown-none-eabi).
#
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/toolchain-ug24.cmake \
#         -DUG24_TOOLCHAIN=$HOME/llvm-arm-cross/build-ug24
#
# UG24_TOOLCHAIN must point at the build or install directory holding bin/ and
# lib/ug24/.  It defaults to the layout this repository builds by default.

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR ug24)

if(NOT DEFINED UG24_TOOLCHAIN)
  set(UG24_TOOLCHAIN "$ENV{HOME}/llvm-arm-cross/build-ug24")
endif()

set(UG24_BIN "${UG24_TOOLCHAIN}/bin")

set(CMAKE_C_COMPILER   "${UG24_BIN}/clang")
set(CMAKE_CXX_COMPILER "${UG24_BIN}/clang++")
set(CMAKE_ASM_COMPILER "${UG24_BIN}/clang")
set(CMAKE_AR           "${UG24_BIN}/llvm-ar")
set(CMAKE_RANLIB       "${UG24_BIN}/llvm-ranlib")
set(CMAKE_OBJCOPY      "${UG24_BIN}/llvm-objcopy")
set(CMAKE_OBJDUMP      "${UG24_BIN}/llvm-objdump")
set(CMAKE_SIZE         "${UG24_BIN}/llvm-size")

set(CMAKE_C_COMPILER_TARGET   ug24-unknown-none-eabi)
set(CMAKE_CXX_COMPILER_TARGET ug24-unknown-none-eabi)
set(CMAKE_ASM_COMPILER_TARGET ug24-unknown-none-eabi)

# There is no operating system, so a bare compile is the only viable probe.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Look for target libraries and headers only under the toolchain, never on the
# host.
set(CMAKE_FIND_ROOT_PATH "${UG24_TOOLCHAIN}/lib/ug24")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
