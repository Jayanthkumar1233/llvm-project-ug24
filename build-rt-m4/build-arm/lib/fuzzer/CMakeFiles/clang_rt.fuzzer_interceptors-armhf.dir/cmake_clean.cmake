file(REMOVE_RECURSE
  "../linux/libclang_rt.fuzzer_interceptors-armhf.a"
  "../linux/libclang_rt.fuzzer_interceptors-armhf.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/clang_rt.fuzzer_interceptors-armhf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
