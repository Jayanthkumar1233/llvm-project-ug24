file(REMOVE_RECURSE
  "libRTOrc.test.armhf.a"
  "libRTOrc.test.armhf.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang ASM CXX)
  include(CMakeFiles/RTOrc.test.armhf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
