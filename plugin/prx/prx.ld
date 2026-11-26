OUTPUT_FORMAT("elf64-powerpc", "elf64-powerpc", "elf64-powerpc")
OUTPUT_ARCH(powerpc:common64)

SECTIONS
{
  . = 0x0;
  .text : {
    *(.text .text.*)
    *(.sceStub.text)
  }
  .rodata : {
    *(.rodata .rodata.*)
    *(.rodata.sceModuleInfo)
    *(.rodata.sceResident)
    *(.rodata.sceFNID)
  }
  .lib.ent : { *(.lib.ent) }
  .lib.stub : { *(.lib.stub) }
  .sys_proc_prx_param : { *(.sys_proc_prx_param) }

  . = ALIGN(0x10000);
  .data : {
    *(.data .data.*)
    *(.rodata.sceFStub)
    *(.opd)
    *(.got)
    *(.toc)
  }
  .bss : {
    *(.bss .bss.*)
    *(COMMON)
  }
}
