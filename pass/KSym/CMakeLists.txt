add_llvm_loadable_module(KSym
  Lower.cpp
  Alias.cpp

  Util.cpp
  Logger.cpp

  Slice.cpp
  DAG.cpp
  Unroll.cpp
  Oracle.cpp

  Symbolic.cpp
  Trace.cpp
  Libcall.cpp
  Asmcall.cpp
  SEG.cpp

  Fetch.cpp
  Func.cpp
  Tool.cpp

  Record.cpp

	Node.cpp
	Global.cpp
	PA.cpp
)

set_target_properties(KSym PROPERTIES
  COMPILE_FLAGS "-fno-rtti"
)
