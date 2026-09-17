file(REMOVE_RECURSE
  "../linux/libclang_rt.xray-armhf.a"
  "../linux/libclang_rt.xray-armhf.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang ASM CXX)
  include(CMakeFiles/clang_rt.xray-armhf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
