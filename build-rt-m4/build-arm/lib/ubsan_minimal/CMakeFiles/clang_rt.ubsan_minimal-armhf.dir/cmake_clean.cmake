file(REMOVE_RECURSE
  "../linux/libclang_rt.ubsan_minimal-armhf.a"
  "../linux/libclang_rt.ubsan_minimal-armhf.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/clang_rt.ubsan_minimal-armhf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
