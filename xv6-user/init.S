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
	.string	"init: exec %s failed\n"
	.align	3
.LC2:
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
	sd	s0,16(sp)
	sd	s2,0(sp)
	sd	s1,8(sp)
	call	dev@plt
	li	a0,0
	call	dup@plt
	li	a0,0
	call	dup@plt
	li	s0,0
	li	s2,29
.L5:
	call	fork@plt
	mv	s1,a0
	bge	a0,zero,.L2
	lla	a0,.LC0
.L11:
	call	printf@plt
	j	.L10
.L2:
	bne	a0,zero,.L3
	lla	a1,.LANCHOR0
	slli	s0,s0,3
	add	s0,a1,s0
	ld	a0,16(s0)
	call	exec@plt
	ld	a1,16(s0)
	lla	a0,.LC1
	call	printf@plt
.L10:
	li	a0,1
	call	exit@plt
.L3:
	li	a0,0
	call	wait@plt
	beq	s1,a0,.L4
	bge	a0,zero,.L3
	lla	a0,.LC2
	j	.L11
.L4:
	addiw	s0,s0,1
	bne	s0,s2,.L5
	call	shutdown@plt
	ld	ra,24(sp)
	ld	s0,16(sp)
	ld	s1,8(sp)
	ld	s2,0(sp)
	li	a0,0
	addi	sp,sp,32
	jr	ra
	.size	main, .-main
	.globl	tests
	.section	.rodata.str1.8
	.align	3
.LC3:
	.string	"openat"
	.align	3
.LC4:
	.string	"open"
	.align	3
.LC5:
	.string	"close"
	.align	3
.LC6:
	.string	"dup"
	.align	3
.LC7:
	.string	"dup2"
	.align	3
.LC8:
	.string	"pipe"
	.align	3
.LC9:
	.string	"getdents"
	.align	3
.LC10:
	.string	"chdir"
	.align	3
.LC11:
	.string	"mkdir"
	.align	3
.LC12:
	.string	"unlink"
	.align	3
.LC13:
	.string	"read"
	.align	3
.LC14:
	.string	"getcwd"
	.align	3
.LC15:
	.string	"getpid"
	.align	3
.LC16:
	.string	"write"
	.align	3
.LC17:
	.string	"times"
	.align	3
.LC18:
	.string	"uname"
	.align	3
.LC19:
	.string	"brk"
	.align	3
.LC20:
	.string	"mmap"
	.align	3
.LC21:
	.string	"munmap"
	.align	3
.LC22:
	.string	"fork"
	.align	3
.LC23:
	.string	"exit"
	.align	3
.LC24:
	.string	"clone"
	.align	3
.LC25:
	.string	"wait"
	.align	3
.LC26:
	.string	"waitpid"
	.align	3
.LC27:
	.string	"yield"
	.align	3
.LC28:
	.string	"execve"
	.align	3
.LC29:
	.string	"getppid"
	.align	3
.LC30:
	.string	"gettimeofday"
	.align	3
.LC31:
	.string	"sleep"
	.globl	argv
	.align	3
.LC32:
	.string	"sh"
	.section	.data.rel.local,"aw"
	.align	3
	.set	.LANCHOR0,. + 0
	.type	argv, @object
	.size	argv, 16
argv:
	.dword	.LC32
	.dword	0
	.type	tests, @object
	.size	tests, 232
tests:
	.dword	.LC3
	.dword	.LC4
	.dword	.LC5
	.dword	.LC6
	.dword	.LC7
	.dword	.LC8
	.dword	.LC9
	.dword	.LC10
	.dword	.LC11
	.dword	.LC12
	.dword	.LC13
	.dword	.LC14
	.dword	.LC15
	.dword	.LC16
	.dword	.LC17
	.dword	.LC18
	.dword	.LC19
	.dword	.LC20
	.dword	.LC21
	.dword	.LC22
	.dword	.LC23
	.dword	.LC24
	.dword	.LC25
	.dword	.LC26
	.dword	.LC27
	.dword	.LC28
	.dword	.LC29
	.dword	.LC30
	.dword	.LC31
	.ident	"GCC: (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0"
	.section	.note.GNU-stack,"",@progbits
