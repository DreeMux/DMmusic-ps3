OUTPUT_FORMAT("elf64-powerpc", "elf64-powerpc", "elf64-powerpc")
OUTPUT_ARCH(powerpc:common64)

PHDRS
{
  text PT_LOAD FLAGS(5);            /* R + E */
  data PT_LOAD FLAGS(6);            /* R + W */
  prx  0x70000002 FLAGS(4);         /* PT_PROC_PRX_PARAM */
}

SECTIONS
{
  . = 0x0;
  .text : {
    *(.text .text.*)
    *(.sceStub.text)
  } :text

  .rodata : {
    *(.rodata .rodata.*)
    *(.rodata.sceModuleInfo)
    *(.rodata.sceResident)
    *(.rodata.sceFNID)
    *(.lib.ent)
    *(.lib.stub)
  } :text

  .sys_proc_prx_param : {
    *(.sys_proc_prx_param)
  } :text :prx

  . = ALIGN(0x10000);
  .data : {
    *(.data .data.*)
    *(.rodata.sceFStub)
    *(.opd)
    *(.got)
    *(.toc)
  } :data

  .bss : {
    *(.bss .bss.*)
    *(COMMON)
  } :data
}
