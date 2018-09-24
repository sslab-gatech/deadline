#include "Project.h"

// option setup 
cl::opt<string> OUTF("symf", cl::Required, cl::desc("<sym file>"));

// pass info
char KSym::ID = 0;
static RegisterPass<KSym> X("KSym", "Kernel Static Symbolizer", false, true);

// globals
Dumper DUMP;

#ifdef KSYM_DEBUG
Logger SLOG;
#endif

set<Function *> EXCEPT;

// black lists
static const set<string> BLACKLIST({
    // llvm loop construction bug
    string("vmw_execbuf_process"),
    string("do_io_rw"),
    string("i915_gem_do_execbuffer"),
    string("atomisp_cp_general_isp_parameters"),
    // user copying
    string("copy_strings"),
    string("mcopy_atomic"),
    string("strndup_user"),
    // sync
    string("do_futex"),
    string("futex_requeue"),
    // core dump
    string("do_usercopy_stack"),
    string("do_usercopy_heap_size"),
    string("do_usercopy_heap_flag"),
    string("elf_core_dump"),
    string("elf_core_dump.2051"),
    // path explosion
    string("pktgen_if_write"),
    string("pktgen_thread_write"),
    // loop explosion
    string("mptctl_do_fw_download"),
    string("qib_user_sdma_writev"),
    string("snd_emu10k1_icode_poke"),
    string("snd_soundfont_load"),
    string("st_write"),
    string("vringh_getdesc_user"),
    // manually checked
    string("__nd_ioctl"),
    });

// class kAA
KSym::KSym() : ModulePass(ID) {
  // do nothing
}

KSym::~KSym() {
#ifdef KSYM_DEBUG
  SLOG.dump(OUTF.getValue());
#endif
}

void KSym::getAnalysisUsage(AnalysisUsage &au) const {
  // since we are one-shot pass, simply mark everything is preserved
  // regardless of whether it is actually preserved or not
  ModulePass::getAnalysisUsage(au);
  au.setPreservesAll();
}

// entry point
bool KSym::runOnModule(Module &m) {
  // run module pass
  breakConstantExpr(m);

  // create module-level vars
  ModuleOracle mo(m);

  // per-function handling
  for(Function &f : m){
    // ignored non-defined functions
    if(f.isIntrinsic() || f.isDeclaration()){
      continue;
    }

    // ignore blacklisted functions
    if(BLACKLIST.find(f.getName().str()) != BLACKLIST.end()){
      continue;
    }

    // lower swtich here
    lowerSwitch(f);

    // create and run function handler
#ifdef KSYM_DEBUG
    SLOG.map(f.getName().str());
#endif

    FuncHandle handle(f, mo);
    handle.run();

#ifdef KSYM_DEBUG
    SLOG.pop();
#endif
  }

  // dump exceptions
  for(Function *ex : EXCEPT){
    errs() << "[!] " << ex->getName() << "\n";
  }

  // mark nothing have changed
  return false;
}

