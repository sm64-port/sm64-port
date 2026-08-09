.set noat

.text
.set push
.set noreorder

/*
 * uint32_t gfx_clip_to_hyperplane_vfpu(
 *     struct LoadedVertex *dest,
 *     const struct LoadedVertex *source,
 *     const float plane[4],
 *     uint32_t inCount);
 */

#define LOADED_VERTEX_SIZE 48
#define LOADED_VERTEX_MODEL_POS 0
#define LOADED_VERTEX_CLIP_POS 16
#define LOADED_VERTEX_UV 32
#define LOADED_VERTEX_V 36
#define LOADED_VERTEX_COLOR 40
#define LOADED_VERTEX_CLIP_REJ 44
#define CLIP_EPSILON_BITS 0x322BCC77

.globl gfx_clip_to_hyperplane_vfpu
gfx_clip_to_hyperplane_vfpu:
	or      $v0, $zero, $zero
	beq     $a3, $zero, .Lfinished_all_vertices
	nop

	lv.q    C000, 0($a2)
	li      $t0, CLIP_EPSILON_BITS
	mtv     $t0, S700

	or      $t2, $a1, $zero
	sll     $t1, $a3, 4
	sll     $t0, $a3, 5
	addu    $t1, $t1, $t0
	addu    $t1, $a1, $t1

	lv.q    C300, LOADED_VERTEX_MODEL_POS($a1)
	lv.q    C310, LOADED_VERTEX_CLIP_POS($a1)
	lv.q    C320, LOADED_VERTEX_UV($a1)
	lw      $t4, LOADED_VERTEX_COLOR($a1)
	addiu   $a1, $a1, LOADED_VERTEX_SIZE

	vdot.q  S702, C310, C000

.Lget_next_vertex:
	bne     $a1, $t1, .Lload_a
	nop
	or      $a1, $t2, $zero

.Lload_a:
	lv.q    C210, LOADED_VERTEX_CLIP_POS($a1)
	lv.q    C200, LOADED_VERTEX_MODEL_POS($a1)
	vdot.q  S701, C210, C000
	lv.q    C220, LOADED_VERTEX_UV($a1)
	vcmp.s  GT, S701, S700
	lw      $t3, LOADED_VERTEX_COLOR($a1)

	bvt     0, .La_is_outside
	nop

.La_is_inside:
	vcmp.s  LE, S702, S700
	bvt     0, .La_is_inside_copy
	nop

	/* b was outside, a is inside: emit the intersection first. */
	vsub.q  C030, C310, C210
	vdot.q  S703, C030, C000
	vrcp.s  S703, S703
	vmul.s  S703, S702, S703

	vsub.q  C100, C200, C300
	vsub.q  C110, C210, C310
	vsub.s  S120, S220, S320
	vsub.s  S121, S221, S321

	vscl.q  C100, C100, S703
	vscl.q  C110, C110, S703
	vmul.s  S120, S120, S703
	vmul.s  S121, S121, S703

	vadd.q  C100, C300, C100
	vadd.q  C110, C310, C110
	vadd.s  S120, S320, S120
	vadd.s  S121, S321, S121

	sv.q    C100, LOADED_VERTEX_MODEL_POS($a0)
	sv.q    C110, LOADED_VERTEX_CLIP_POS($a0)
	sv.s    S120, LOADED_VERTEX_UV($a0)
	sv.s    S121, LOADED_VERTEX_V($a0)
	mfv     $t0, S703
	mtc1    $t0, $f0
	b       .Linterpolate_color
	nop

.La_is_inside_after_intersection:
	addiu   $v0, $v0, 1
	addiu   $a0, $a0, LOADED_VERTEX_SIZE

.La_is_inside_copy:
	sv.q    C200, LOADED_VERTEX_MODEL_POS($a0)
	sv.q    C210, LOADED_VERTEX_CLIP_POS($a0)
	sv.q    C220, LOADED_VERTEX_UV($a0)

	addiu   $a0, $a0, LOADED_VERTEX_SIZE
	b       .Lfinished_vertex
	addiu   $v0, $v0, 1

.La_is_outside:
	vcmp.s  GT, S702, S700
	bvt     0, .Lfinished_vertex
	nop

	/* b was inside, a is outside: emit only the intersection. */
	vsub.q  C030, C310, C210
	vdot.q  S703, C030, C000
	vrcp.s  S703, S703
	vmul.s  S703, S702, S703

	vsub.q  C100, C200, C300
	vsub.q  C110, C210, C310
	vsub.s  S120, S220, S320
	vsub.s  S121, S221, S321

	vscl.q  C100, C100, S703
	vscl.q  C110, C110, S703
	vmul.s  S120, S120, S703
	vmul.s  S121, S121, S703

	vadd.q  C100, C300, C100
	vadd.q  C110, C310, C110
	vadd.s  S120, S320, S120
	vadd.s  S121, S321, S121

	sv.q    C100, LOADED_VERTEX_MODEL_POS($a0)
	sv.q    C110, LOADED_VERTEX_CLIP_POS($a0)
	sv.s    S120, LOADED_VERTEX_UV($a0)
	sv.s    S121, LOADED_VERTEX_V($a0)
	mfv     $t0, S703
	mtc1    $t0, $f0
	b       .Linterpolate_color
	nop

.La_is_outside_after_intersection:
	addiu   $v0, $v0, 1
	addiu   $a0, $a0, LOADED_VERTEX_SIZE
	b       .Lfinished_vertex
	nop

.Linterpolate_color:
	or      $t5, $zero, $zero

	andi    $t6, $t4, 0x00FF
	andi    $t7, $t3, 0x00FF
	mtc1    $t6, $f2
	mtc1    $t7, $f4
	cvt.s.w $f2, $f2
	cvt.s.w $f4, $f4
	sub.s   $f4, $f4, $f2
	mul.s   $f4, $f4, $f0
	add.s   $f4, $f4, $f2
	trunc.w.s $f4, $f4
	mfc1    $t6, $f4
	andi    $t6, $t6, 0x00FF
	or      $t5, $t5, $t6

	srl     $t6, $t4, 8
	srl     $t7, $t3, 8
	andi    $t6, $t6, 0x00FF
	andi    $t7, $t7, 0x00FF
	mtc1    $t6, $f2
	mtc1    $t7, $f4
	cvt.s.w $f2, $f2
	cvt.s.w $f4, $f4
	sub.s   $f4, $f4, $f2
	mul.s   $f4, $f4, $f0
	add.s   $f4, $f4, $f2
	trunc.w.s $f4, $f4
	mfc1    $t6, $f4
	andi    $t6, $t6, 0x00FF
	sll     $t6, $t6, 8
	or      $t5, $t5, $t6

	srl     $t6, $t4, 16
	srl     $t7, $t3, 16
	andi    $t6, $t6, 0x00FF
	andi    $t7, $t7, 0x00FF
	mtc1    $t6, $f2
	mtc1    $t7, $f4
	cvt.s.w $f2, $f2
	cvt.s.w $f4, $f4
	sub.s   $f4, $f4, $f2
	mul.s   $f4, $f4, $f0
	add.s   $f4, $f4, $f2
	trunc.w.s $f4, $f4
	mfc1    $t6, $f4
	andi    $t6, $t6, 0x00FF
	sll     $t6, $t6, 16
	or      $t5, $t5, $t6

	srl     $t6, $t4, 24
	srl     $t7, $t3, 24
	andi    $t6, $t6, 0x00FF
	andi    $t7, $t7, 0x00FF
	mtc1    $t6, $f2
	mtc1    $t7, $f4
	cvt.s.w $f2, $f2
	cvt.s.w $f4, $f4
	sub.s   $f4, $f4, $f2
	mul.s   $f4, $f4, $f0
	add.s   $f4, $f4, $f2
	trunc.w.s $f4, $f4
	mfc1    $t6, $f4
	andi    $t6, $t6, 0x00FF
	sll     $t6, $t6, 24
	or      $t5, $t5, $t6

	sw      $t5, LOADED_VERTEX_COLOR($a0)
	sw      $zero, LOADED_VERTEX_CLIP_REJ($a0)

	vcmp.s  GT, S701, S700
	bvt     0, .La_is_outside_after_intersection
	nop
	b       .La_is_inside_after_intersection
	nop

.Lfinished_vertex:
	vmov.q  C300, C200
	vmov.q  C310, C210
	vmov.q  C320, C220
	or      $t4, $t3, $zero
	vmov.s  S702, S701

	addiu   $a3, $a3, -1
	bne     $a3, $zero, .Lget_next_vertex
	addiu   $a1, $a1, LOADED_VERTEX_SIZE

.Lfinished_all_vertices:
	jr      $ra
	nop

.set pop
