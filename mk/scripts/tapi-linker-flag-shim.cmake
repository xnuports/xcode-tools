# --- xcode_tools_llvm_check_linker_flag_shim ---
# llvm_check_linker_flag() lived in LLVM's LLVMCheckLinkerFlag module,
# which current llvm-project no longer ships; forward it to CMake's own
# check_linker_flag.  Inserted by mk/scripts/tapi-shim-linker-flag.sh.
if(NOT COMMAND llvm_check_linker_flag)
  include(CheckLinkerFlag)
  function(llvm_check_linker_flag lang flag out)
    check_linker_flag("${lang}" "${flag}" ${out})
    set(${out} "${${out}}" PARENT_SCOPE)
  endfunction()
endif()
# --- end shim ---
