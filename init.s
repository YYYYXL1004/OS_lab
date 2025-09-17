	.file	"init.c"
	.option pic
	.option norelax
	.text
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align	3
.LC0:
	.string	"init: starting sh\n"
	.align	3
.LC1:
	.string	"init: fork failed\n"
	.align	3
.LC2:
	.string	"sh"
	.align	3
.LC3:
	.string	"init: exec sh failed\n"
	.align	3
.LC4:
	.string	"init: wait returned an error\n"
	.section	.text.startup,"ax",@progbits
	.align	1
	.globl	main
	.type	main, @function
main:
	addi	sp,sp,-32
	li	a2,0
	li	a1,1
	li	a0,2
	sd	ra,24(sp)
	sd	s1,8(sp)
	sd	s0,16(sp)
	call	dev@plt
	li	a0,0
	call	dup@plt
	li	a0,0
	call	dup@plt
	lla	s1,.LC0
.L4:
	mv	a0,s1
	call	printf@plt
	call	fork@plt
	mv	s0,a0
	bge	a0,zero,.L2
	lla	a0,.LC1
.L9:
	call	printf@plt
	li	a0,1
	call	exit@plt
.L2:
	bne	a0,zero,.L3
	lla	a1,.LANCHOR0
	lla	a0,.LC2
	call	exec@plt
	lla	a0,.LC3
	j	.L9
.L3:
	li	a0,0
	call	wait@plt
	beq	s0,a0,.L4
	bge	a0,zero,.L3
	lla	a0,.LC4
	j	.L9
	.size	main, .-main
	.globl	argv
	.section	.data.rel.local,"aw"
	.align	3
	.set	.LANCHOR0,. + 0
	.type	argv, @object
	.size	argv, 16
argv:
	.dword	.LC2
	.dword	0
	.ident	"GCC: (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0"
	.section	.note.GNU-stack,"",@progbits
