file(REMOVE_RECURSE
  "../linux/libclang_rt.ubsan_standalone-armhf.pdb"
  "../linux/libclang_rt.ubsan_standalone-armhf.so"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/clang_rt.ubsan_standalone-dynamic-armhf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
