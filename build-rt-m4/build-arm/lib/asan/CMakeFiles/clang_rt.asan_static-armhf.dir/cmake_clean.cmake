file(REMOVE_RECURSE
  "../linux/libclang_rt.asan_static-armhf.a"
  "../linux/libclang_rt.asan_static-armhf.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/clang_rt.asan_static-armhf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
