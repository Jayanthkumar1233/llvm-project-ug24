# Install script for directory: /home/basil-16/llvm-arm-cross/llvm-project/compiler-rt/lib/scudo/standalone

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/home/basil-16/llvm-arm-cross/toolchain-armhf")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/home/basil-16/llvm-arm-cross/stage1/bin/llvm-objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "clang_rt.scudo_standalone-armhf" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/linux" TYPE STATIC_LIBRARY FILES "/home/basil-16/llvm-arm-cross/llvm-project/build-rt-armhf/lib/linux/libclang_rt.scudo_standalone-armhf.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "clang_rt.scudo_standalone_cxx-armhf" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/linux" TYPE STATIC_LIBRARY FILES "/home/basil-16/llvm-arm-cross/llvm-project/build-rt-armhf/lib/linux/libclang_rt.scudo_standalone_cxx-armhf.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "clang_rt.scudo_standalone-dynamic-armhf" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/linux/libclang_rt.scudo_standalone-armhf.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/linux/libclang_rt.scudo_standalone-armhf.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/linux/libclang_rt.scudo_standalone-armhf.so"
         RPATH "")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/linux" TYPE SHARED_LIBRARY FILES "/home/basil-16/llvm-arm-cross/llvm-project/build-rt-armhf/lib/linux/libclang_rt.scudo_standalone-armhf.so")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/linux/libclang_rt.scudo_standalone-armhf.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/linux/libclang_rt.scudo_standalone-armhf.so")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/home/basil-16/llvm-arm-cross/stage1/bin/llvm-strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/linux/libclang_rt.scudo_standalone-armhf.so")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "clang_rt.scudo_standalone-dynamic-armhf" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/basil-16/llvm-arm-cross/llvm-project/build-rt-armhf/lib/scudo/standalone/benchmarks/cmake_install.cmake")
endif()

