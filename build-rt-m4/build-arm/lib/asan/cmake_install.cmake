# Install script for directory: /home/basil-16/llvm-arm-cross/llvm-project/compiler-rt/lib/asan

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
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
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/home/basil-16/llvm-arm-cross/toolchain-armhf/bin/llvm-objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "clang_rt.asan-armhf" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/linux" TYPE STATIC_LIBRARY FILES "/home/basil-16/llvm-arm-cross/llvm-project/build-rt-m4/build-arm/lib/linux/libclang_rt.asan-armhf.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "clang_rt.asan_cxx-armhf" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/linux" TYPE STATIC_LIBRARY FILES "/home/basil-16/llvm-arm-cross/llvm-project/build-rt-m4/build-arm/lib/linux/libclang_rt.asan_cxx-armhf.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "clang_rt.asan_static-armhf" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/linux" TYPE STATIC_LIBRARY FILES "/home/basil-16/llvm-arm-cross/llvm-project/build-rt-m4/build-arm/lib/linux/libclang_rt.asan_static-armhf.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "clang_rt.asan-preinit-armhf" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/linux" TYPE STATIC_LIBRARY FILES "/home/basil-16/llvm-arm-cross/llvm-project/build-rt-m4/build-arm/lib/linux/libclang_rt.asan-preinit-armhf.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "clang_rt.asan-dynamic-armhf" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/linux/libclang_rt.asan-armhf.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/linux/libclang_rt.asan-armhf.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/linux/libclang_rt.asan-armhf.so"
         RPATH "")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/linux" TYPE SHARED_LIBRARY FILES "/home/basil-16/llvm-arm-cross/llvm-project/build-rt-m4/build-arm/lib/linux/libclang_rt.asan-armhf.so")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/linux/libclang_rt.asan-armhf.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/linux/libclang_rt.asan-armhf.so")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/home/basil-16/llvm-arm-cross/toolchain-armhf/bin/llvm-strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/linux/libclang_rt.asan-armhf.so")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "clang_rt.asan-dynamic-armhf" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/linux" TYPE FILE FILES "/home/basil-16/llvm-arm-cross/llvm-project/build-rt-m4/build-arm/lib/linux/libclang_rt.asan_cxx-armhf.a.syms")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/linux" TYPE FILE FILES "/home/basil-16/llvm-arm-cross/llvm-project/build-rt-m4/build-arm/lib/linux/libclang_rt.asan-armhf.a.syms")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "asan" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE FILE FILES "/home/basil-16/llvm-arm-cross/llvm-project/compiler-rt/lib/asan/asan_ignorelist.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/basil-16/llvm-arm-cross/llvm-project/build-rt-m4/build-arm/lib/asan/scripts/cmake_install.cmake")
endif()

