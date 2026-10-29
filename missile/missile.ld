/* missile.ld -- the blob is one RWX region at a fixed guest address.
 *
 * 0x10300000 is past the end of the library image (0x1013341c) and close
 * enough to it that a Thumb `bl` from any patch site reaches (+-4 MB): the
 * lowest site, 0x10028b6e, reaches 0x1042870c, past the whole blob. Both
 * runtimes map BLOB_SIZE bytes here (1 MB since 2026-10-12, 512 KB before; nothing else is mapped below
 * the heap at 0x40000000), zero-filled, so .bss needs no copying. Code
 * comes first; big tables (worlds.c's script pool) sit in .bss at the end,
 * where no `bl` has to reach.
 */
ENTRY(hook_update_spread)
SECTIONS
{
  . = 0x10300000;
  .text   : { *(.text .text.*) }
  .rodata : { *(.rodata .rodata.*) }
  .data   : { *(.data .data.*) }
  .bss    : { *(.bss .bss.* COMMON) }
  __blob_end = .;
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.ARM.attributes) }
}
