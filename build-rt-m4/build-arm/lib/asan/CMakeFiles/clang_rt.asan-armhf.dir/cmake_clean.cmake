file(REMOVE_RECURSE
  "../linux/libclang_rt.asan-armhf.a"
  "../linux/libclang_rt.asan-armhf.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang ASM CXX)
  include(CMakeFiles/clang_rt.asan-armhf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
