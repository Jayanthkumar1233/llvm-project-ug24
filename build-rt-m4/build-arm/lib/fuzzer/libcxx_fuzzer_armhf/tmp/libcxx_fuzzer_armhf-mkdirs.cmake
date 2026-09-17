# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/basil-16/llvm-arm-cross/llvm-project/llvm/../runtimes"
  "/home/basil-16/llvm-arm-cross/llvm-project/build-rt-m4/build-arm/lib/fuzzer/libcxx_fuzzer_armhf"
  "/home/basil-16/llvm-arm-cross/llvm-project/build-rt-m4/build-arm/lib/fuzzer/libcxx_fuzzer_armhf"
  "/home/basil-16/llvm-arm-cross/llvm-project/build-rt-m4/build-arm/lib/fuzzer/libcxx_fuzzer_armhf/tmp"
  "/home/basil-16/llvm-arm-cross/llvm-project/build-rt-m4/build-arm/lib/fuzzer/libcxx_fuzzer_armhf/src/libcxx_fuzzer_armhf-stamp"
  "/home/basil-16/llvm-arm-cross/llvm-project/build-rt-m4/build-arm/lib/fuzzer/libcxx_fuzzer_armhf/src"
  "/home/basil-16/llvm-arm-cross/llvm-project/build-rt-m4/build-arm/lib/fuzzer/libcxx_fuzzer_armhf/src/libcxx_fuzzer_armhf-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/basil-16/llvm-arm-cross/llvm-project/build-rt-m4/build-arm/lib/fuzzer/libcxx_fuzzer_armhf/src/libcxx_fuzzer_armhf-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/basil-16/llvm-arm-cross/llvm-project/build-rt-m4/build-arm/lib/fuzzer/libcxx_fuzzer_armhf/src/libcxx_fuzzer_armhf-stamp${cfgdir}") # cfgdir has leading slash
endif()
