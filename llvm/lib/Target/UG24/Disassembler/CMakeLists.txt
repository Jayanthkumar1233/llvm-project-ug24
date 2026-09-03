add_llvm_component_library(LLVMUG24Disassembler
  UG24Disassembler.cpp

  LINK_COMPONENTS
  MCDisassembler
  Support
  UG24Desc
  UG24Info

  ADD_TO_COMPONENT
  UG24
  )
