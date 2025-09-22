	.file	"init.c"
	.option pic
	.option norelax
	.text
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align	3
.LC0:
	.string	"init: fork failed\n"
	.align	3
.LC1:
	.string	"\347\254\254%d\346\254\241"
	.align	3
.LC2:
	.string	"init: exec %s failed\n"
	.align	3
.LC3:
	.string	"init: wait returned an error\n"
	.section	.text.startup,"ax",@progbits
	.align	1
	.globl	main
	.type	main, @function
main:
	addi	sp,sp,-16
	li	a2,0
	li	a1,1
	li	a0,2
	sd	ra,8(sp)
	sd	s0,0(sp)
	call	dev@plt
	li	a0,0
	call	dup@plt
	li	a0,0
	call	dup@plt
	call	fork@plt
	blt	a0,zero,.L2
	mv	s0,a0
	beq	a0,zero,.L3
.L6:
	li	a0,0
	call	wait@plt
	beq	s0,a0,.L4
	bge	a0,zero,.L6
.L5:
	lla	a0,.LC3
	j	.L17
.L4:
	call	fork@plt
	mv	s0,a0
	bge	a0,zero,.L7
.L2:
	lla	a0,.LC0
.L17:
	call	printf@plt
	j	.L16
.L7:
	bne	a0,zero,.L8
	li	s0,1
.L3:
	mv	a1,s0
	lla	a0,.LC1
	call	printf@plt
	lla	a1,.LANCHOR0
	slli	s0,s0,3
	add	s0,a1,s0
	ld	a0,16(s0)
	call	exec@plt
	ld	a1,16(s0)
	lla	a0,.LC2
	call	printf@plt
.L16:
	li	a0,1
	call	exit@plt
.L8:
	li	a0,0
	call	wait@plt
	beq	s0,a0,.L9
	bge	a0,zero,.L8
	j	.L5
.L9:
	call	shutdown@plt
	ld	ra,8(sp)
	ld	s0,0(sp)
	li	a0,0
	addi	sp,sp,16
	jr	ra
	.size	main, .-main
	.globl	tests
	.section	.rodata.str1.8
	.align	3
.LC4:
	.string	"getpid"
	.align	3
.LC5:
	.string	"write"
	.globl	argv
	.align	3
.LC6:
	.string	"sh"
	.section	.data.rel.local,"aw"
	.align	3
	.set	.LANCHOR0,. + 0
	.type	argv, @object
	.size	argv, 16
argv:
	.dword	.LC6
	.dword	0
	.type	tests, @object
	.size	tests, 16
tests:
	.dword	.LC4
	.dword	.LC5
	.ident	"GCC: (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0"
	.section	.note.GNU-stack,"",@progbits
