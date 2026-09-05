add_llvm_component_group(UG24)

set(LLVM_TARGET_DEFINITIONS UG24.td)
tablegen(LLVM UG24GenRegisterInfo.inc -gen-register-info)
tablegen(LLVM UG24GenInstrInfo.inc -gen-instr-info)
tablegen(LLVM UG24GenCallingConv.inc -gen-callingconv)
tablegen(LLVM UG24GenSubtargetInfo.inc -gen-subtarget)
tablegen(LLVM UG24GenDAGISel.inc -gen-dag-isel)
tablegen(LLVM UG24GenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM UG24GenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM UG24GenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM UG24GenMCCodeEmitter.inc -gen-emitter)
add_public_tablegen_target(UG24CommonTableGen)

add_llvm_target(UG24CodeGen
  UG24AsmPrinter.cpp
  UG24ExpandPseudo.cpp
  UG24FrameLowering.cpp
  UG24InstrInfo.cpp
  UG24ISelDAGToDAG.cpp
  UG24ISelLowering.cpp
  UG24MCInstLower.cpp
  UG24RegisterInfo.cpp
  UG24SelectionDAGInfo.cpp
  UG24Subtarget.cpp
  UG24TargetMachine.cpp

  LINK_COMPONENTS
  Analysis
  AsmPrinter
  CodeGen
  Core
  MC
  SelectionDAG
  Support
  Target
  TargetParser
  UG24Desc
  UG24Info

  ADD_TO_COMPONENT
  UG24
  )

add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
