file(REMOVE_RECURSE
  "../linux/libclang_rt.xray-basic-armhf.a"
  "../linux/libclang_rt.xray-basic-armhf.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/clang_rt.xray-basic-armhf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
