add_llvm_component_library(LLVMUG24AsmParser
  UG24AsmParser.cpp

  LINK_COMPONENTS
  MC
  MCParser
  Support
  UG24Desc
  UG24Info

  ADD_TO_COMPONENT
  UG24
  )
