file(REMOVE_RECURSE
  "../linux/libclang_rt.xray-fdr-armhf.a"
  "../linux/libclang_rt.xray-fdr-armhf.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/clang_rt.xray-fdr-armhf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
