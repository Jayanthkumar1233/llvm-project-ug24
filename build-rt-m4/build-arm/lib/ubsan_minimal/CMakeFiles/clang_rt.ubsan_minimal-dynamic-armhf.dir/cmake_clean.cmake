file(REMOVE_RECURSE
  "../linux/libclang_rt.ubsan_minimal-armhf.pdb"
  "../linux/libclang_rt.ubsan_minimal-armhf.so"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/clang_rt.ubsan_minimal-dynamic-armhf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
