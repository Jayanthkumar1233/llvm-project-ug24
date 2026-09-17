file(REMOVE_RECURSE
  "../linux/libclang_rt.asan-armhf.pdb"
  "../linux/libclang_rt.asan-armhf.so"
)

# Per-language clean rules from dependency scanning.
foreach(lang ASM CXX)
  include(CMakeFiles/clang_rt.asan-dynamic-armhf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
