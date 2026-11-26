/*
 * DMmusic - PS3 VSH background mp3 player plugin
 * Made by DreeMux — Made for PS3 gamers
 *
 * Copyright (C) DreeMux (https://github.com/DreeMux)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * PRX import stubs (cellAudio, sys_io, sysPrxForUser) with verified NIDs.
 * Symbols are .weak so a strong definition from liblv2.a wins.
 */

	.file	"vsh_stubs.S"

	.macro	IMPORT_FUNC name, fstub
	.section .sceStub.text, "ax", @progbits
	.align	3
.L\name\()_stub:
	lis	%r12, \fstub@ha
	lwz	%r12, \fstub@l(%r12)
	std	%r2, 0x28(%r1)
	lwz	%r0, 0(%r12)
	lwz	%r2, 4(%r12)
	mtctr	%r0
	bctr

	.section .opd, "aw", @progbits
	.align	3
	.weak	\name
	.type	\name, @function
\name:
	.quad	.L\name\()_stub
	.quad	.TOC.@tocbase
	.quad	0
	.size	\name, 24
	.endm

/* ---------------- cellAudio ---------------- */
	.section .rodata.sceResident, "a", @progbits
.Llib_cellAudio_name:
	.asciz	"cellAudio"

	.section .rodata.sceFNID, "a", @progbits
	.align	2
.Lcellaudio_fnid:
	.long	0x0B168F92	/* cellAudioInit          */
	.long	0xCA5AC370	/* cellAudioQuit          */
	.long	0xCD7BC431	/* cellAudioPortOpen      */
	.long	0x4129FE2D	/* cellAudioPortStart     */
	.long	0x5B1E2C73	/* cellAudioPortStop      */
	.long	0x4109D08C	/* cellAudioPortClose     */
	.long	0x74A66AF0	/* cellAudioGetPortConfig */

	.section .rodata.sceFStub, "aw", @progbits
	.align	2
.Lcellaudio_fstub:
.Lfs_cellAudioInit:		.long	0
.Lfs_cellAudioQuit:		.long	0
.Lfs_cellAudioPortOpen:		.long	0
.Lfs_cellAudioPortStart:	.long	0
.Lfs_cellAudioPortStop:		.long	0
.Lfs_cellAudioPortClose:	.long	0
.Lfs_cellAudioGetPortConfig:	.long	0

	.section .lib.stub, "a", @progbits
	.align	2
	.byte	0x2C
	.byte	0
	.short	0x0001
	.short	0x0000
	.short	7
	.short	0
	.short	0
	.byte	0, 0, 0, 0
	.long	.Llib_cellAudio_name
	.long	.Lcellaudio_fnid
	.long	.Lcellaudio_fstub
	.long	0, 0, 0, 0

	IMPORT_FUNC cellAudioInit,		.Lfs_cellAudioInit
	IMPORT_FUNC cellAudioQuit,		.Lfs_cellAudioQuit
	IMPORT_FUNC cellAudioPortOpen,		.Lfs_cellAudioPortOpen
	IMPORT_FUNC cellAudioPortStart,		.Lfs_cellAudioPortStart
	IMPORT_FUNC cellAudioPortStop,		.Lfs_cellAudioPortStop
	IMPORT_FUNC cellAudioPortClose,		.Lfs_cellAudioPortClose
	IMPORT_FUNC cellAudioGetPortConfig,	.Lfs_cellAudioGetPortConfig

/* ---------------- sys_io (cellPad) ---------------- */
	.section .rodata.sceResident, "a", @progbits
.Llib_sys_io_name:
	.asciz	"sys_io"

	.section .rodata.sceFNID, "a", @progbits
	.align	2
.Lsysio_fnid:
	.long	0x8B72CDA2	/* cellPadGetData  */
	.long	0xA703A51D	/* cellPadGetInfo2 */

	.section .rodata.sceFStub, "aw", @progbits
	.align	2
.Lsysio_fstub:
.Lfs_cellPadGetData:		.long	0
.Lfs_cellPadGetInfo2:		.long	0

	.section .lib.stub, "a", @progbits
	.align	2
	.byte	0x2C
	.byte	0
	.short	0x0001
	.short	0x0000
	.short	2
	.short	0
	.short	0
	.byte	0, 0, 0, 0
	.long	.Llib_sys_io_name
	.long	.Lsysio_fnid
	.long	.Lsysio_fstub
	.long	0, 0, 0, 0

	IMPORT_FUNC cellPadGetData,	.Lfs_cellPadGetData
	IMPORT_FUNC cellPadGetInfo2,	.Lfs_cellPadGetInfo2

/* ---------------- sysPrxForUser ---------------- */
	.section .rodata.sceResident, "a", @progbits
.Llib_sysPrxForUser_name:
	.asciz	"sysPrxForUser"

	.section .rodata.sceFNID, "a", @progbits
	.align	2
.Lspfu_fnid:
	.long	0x9F04F7AF	/* _sys_printf                  */
	.long	0xA2C7BA64	/* sys_prx_exitspawn_with_level */

	.section .rodata.sceFStub, "aw", @progbits
	.align	2
.Lspfu_fstub:
.Lfs__sys_printf:			.long	0
.Lfs_sys_prx_exitspawn_with_level:	.long	0

	.section .lib.stub, "a", @progbits
	.align	2
	.byte	0x2C
	.byte	0
	.short	0x0001
	.short	0x0000
	.short	2
	.short	0
	.short	0
	.byte	0, 0, 0, 0
	.long	.Llib_sysPrxForUser_name
	.long	.Lspfu_fnid
	.long	.Lspfu_fstub
	.long	0, 0, 0, 0

	IMPORT_FUNC _sys_printf,			.Lfs__sys_printf
	IMPORT_FUNC sys_prx_exitspawn_with_level,	.Lfs_sys_prx_exitspawn_with_level

/* ---------------- PRX Module Exports (module_start / module_stop) ---------------- */
        .section .rodata.sceFNID, "a", @progbits
        .align 2
.Lexport_nid:
        .long 0xBC9A0086        /* module_start */
        .long 0xAB779874        /* module_stop  */

        .section .rodata.sceFStub, "aw", @progbits
        .align 2
.Lexport_func:
        .long module_start
        .long module_stop

        .section .lib.ent, "a", @progbits
        .align 2
        .byte 0x1C              /* struct size = 28 bytes */
        .byte 0
        .short 0x0001
        .short 0x8000           /* SYS_LIB_AUTO_EXPORT */
        .short 2                /* 2 exported functions */
        .short 0                /* 0 variables */
        .short 0
        .byte 0, 0, 0, 0
        .long 0                 /* NULL name = resident module entry table */
        .long .Lexport_nid
        .long .Lexport_func

	.section .note.GNU-stack, "", @progbits
