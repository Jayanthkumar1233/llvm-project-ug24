set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
 
set(TRIPLE   armv7-linux-gnueabihf)              # NEW
set(SYSROOT  /usr/arm-linux-gnueabihf)                     # NEW - was undefined in v1.0
set(LLVMBIN  /home/basil-16/llvm-arm-cross/toolchain-armhf/bin)      # CHANGED - was a literal placeholder path
 
set(CMAKE_C_COMPILER    ${LLVMBIN}/clang)
set(CMAKE_CXX_COMPILER  ${LLVMBIN}/clang++)
set(CMAKE_LINKER        ${LLVMBIN}/ld.lld)
set(CMAKE_AR            ${LLVMBIN}/llvm-ar)
set(CMAKE_RANLIB        ${LLVMBIN}/llvm-ranlib)
set(CMAKE_STRIP         ${LLVMBIN}/llvm-strip)
 
set(CMAKE_C_FLAGS_INIT   "--target=${TRIPLE} --sysroot=${SYSROOT} --gcc-toolchain=/usr -fuse-ld=lld")
set(CMAKE_CXX_FLAGS_INIT "--target=${TRIPLE} --sysroot=${SYSROOT} --gcc-toolchain=/usr -fuse-ld=lld")
 
set(CMAKE_FIND_ROOT_PATH ${SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM  NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY  ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE  ONLY)
