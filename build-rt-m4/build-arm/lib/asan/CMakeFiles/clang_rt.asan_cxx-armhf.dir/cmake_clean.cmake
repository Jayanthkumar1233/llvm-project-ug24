file(REMOVE_RECURSE
  "../linux/libclang_rt.asan_cxx-armhf.a"
  "../linux/libclang_rt.asan_cxx-armhf.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/clang_rt.asan_cxx-armhf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
