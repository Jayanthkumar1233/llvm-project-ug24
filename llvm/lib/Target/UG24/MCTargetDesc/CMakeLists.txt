add_llvm_component_library(LLVMUG24Desc
  UG24AsmBackend.cpp
  UG24ELFObjectWriter.cpp
  UG24InstPrinter.cpp
  UG24MCAsmInfo.cpp
  UG24MCCodeEmitter.cpp
  UG24MCExpr.cpp
  UG24MCTargetDesc.cpp

  LINK_COMPONENTS
  MC
  Support
  TargetParser
  UG24Info

  ADD_TO_COMPONENT
  UG24
  )
