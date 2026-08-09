.set noat

.text
.set push
.set noreorder

/*
 * void gfx_transform_vertices_vfpu(
 *     struct LoadedVertex *dest,
 *     const Vtx *source,
 *     uint32_t count,
 *     const float matrix[4][4]);
 *
 * Vtx is 16 bytes, with signed object coordinates at offsets 0, 2, and 4.
 * LoadedVertex is 48 bytes; its model and clip vectors start at 0 and 16.
 * The renderer uses row vectors, so the VFPU dot products use matrix columns.
 */

#ifdef F3DEX_GBI_2E
#define VTX_SIZE 24
#else
#define VTX_SIZE 16
#endif
#define LOADED_VERTEX_SIZE 48
#define LOADED_VERTEX_MODEL_POS 0
#define LOADED_VERTEX_CLIP_POS 16

.globl gfx_transform_vertices_vfpu
gfx_transform_vertices_vfpu:
	beq     $a2, $zero, .Ltransform_done
	nop

	/* Combined model-view-projection matrix columns. */
	lv.s    S000, 0($a3)
	lv.s    S001, 16($a3)
	lv.s    S002, 32($a3)
	lv.s    S003, 48($a3)
	lv.s    S010, 4($a3)
	lv.s    S011, 20($a3)
	lv.s    S012, 36($a3)
	lv.s    S013, 52($a3)
	lv.s    S020, 8($a3)
	lv.s    S021, 24($a3)
	lv.s    S022, 40($a3)
	lv.s    S023, 56($a3)
	lv.s    S030, 12($a3)
	lv.s    S031, 28($a3)
	lv.s    S032, 44($a3)
	lv.s    S033, 60($a3)

.Ltransform_vertex:
#ifdef F3DEX_GBI_2E
	lv.s    S200, 0($a1)
	lv.s    S201, 4($a1)
	lv.s    S202, 8($a1)
#else
	lh      $t0, 0($a1)
	lh      $t1, 2($a1)
	lh      $t2, 4($a1)
	mtv     $t0, S200
	mtv     $t1, S201
	mtv     $t2, S202
	vi2f.s  S200, S200, 0
	vi2f.s  S201, S201, 0
	vi2f.s  S202, S202, 0
#endif
	vone.s  S203

	vdot.q  S300, C000, C200
	vdot.q  S301, C010, C200
	vdot.q  S302, C020, C200
	vdot.q  S303, C030, C200

	sv.q    C200, LOADED_VERTEX_MODEL_POS($a0)
	sv.q    C300, LOADED_VERTEX_CLIP_POS($a0)

	addiu   $a1, $a1, VTX_SIZE
	addiu   $a0, $a0, LOADED_VERTEX_SIZE
	addiu   $a2, $a2, -1
	bne     $a2, $zero, .Ltransform_vertex
	nop

.Ltransform_done:
	jr      $ra
	nop

.set pop
