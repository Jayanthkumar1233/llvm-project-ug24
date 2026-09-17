file(REMOVE_RECURSE
  "../linux/liborc_rt-armhf.a"
  "../linux/liborc_rt-armhf.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang ASM CXX)
  include(CMakeFiles/orc_rt-armhf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
