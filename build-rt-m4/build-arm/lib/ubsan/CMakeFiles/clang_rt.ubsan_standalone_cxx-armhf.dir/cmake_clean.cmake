file(REMOVE_RECURSE
  "../linux/libclang_rt.ubsan_standalone_cxx-armhf.a"
  "../linux/libclang_rt.ubsan_standalone_cxx-armhf.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/clang_rt.ubsan_standalone_cxx-armhf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
