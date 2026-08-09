    .set noreorder

#include "pspstub.s"

/* Keep libme-core's kcall import descriptor alive when the PRX link removes
 * otherwise-unreferenced sections. This mirrors the working OOT ME setup. */
    .section .rodata.sceResident, "a"
    .word 0
.Lsm64PspMeKcallModuleName:
    .asciz "kcall"
    .align 2

    .section .lib.stub, "a", @progbits
    .globl sm64PspMeKcallImport
    .type sm64PspMeKcallImport, @object
sm64PspMeKcallImport:
    .word .Lsm64PspMeKcallModuleName
    .word 0x40090000
    .word 0x00030005
    .word .Lsm64PspMeKcallNids
    .word .Lsm64PspMeKcallStubs
    .size sm64PspMeKcallImport, . - sm64PspMeKcallImport

    .section .rodata.sceNid, "a"
.Lsm64PspMeKcallNids:

    .section .sceStub.text, "ax", @progbits
.Lsm64PspMeKcallStubs:

    STUB_FUNC 0x75F43FF0, kcall_2
    STUB_FUNC 0xBADD8D3B, kcall_3
    STUB_FUNC 0x2BB46CB6, kinit
