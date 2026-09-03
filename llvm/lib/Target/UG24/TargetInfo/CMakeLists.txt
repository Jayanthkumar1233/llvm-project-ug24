add_llvm_component_library(LLVMUG24Info
  UG24TargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support
  TargetParser

  ADD_TO_COMPONENT
  UG24
  )
