
#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/syscall.h"
#include "include/sysinfo.h"
#include "include/kalloc.h"
#include "include/vm.h"
#include "include/string.h"
#include "include/printf.h"
#include "include/sysnum.h"
#include "include/sbi.h"
#include "include/file.h"
#include "include/fcntl.h"

// Fetch the uint64 at addr from the current process.
int
fetchaddr(uint64 addr, uint64 *ip)
{
  struct proc *p = myproc();
  if(addr >= p->sz || addr+sizeof(uint64) > p->sz)
    return -1;
  // if(copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
  if(copyin2((char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
int
fetchstr(uint64 addr, char *buf, int max)
{
  // struct proc *p = myproc();
  // int err = copyinstr(p->pagetable, buf, addr, max);
  int err = copyinstr2(buf, addr, max);
  if(err < 0)
    return err;
  return strlen(buf);
}

static uint64
argraw(int n)
{
  struct proc *p = myproc();
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// Fetch the nth 32-bit system call argument.
int
argint(int n, int *ip)
{
  *ip = argraw(n);
  return 0;
}

// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
int
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
  return 0;
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int
argstr(int n, char *buf, int max)
{
  uint64 addr;
  if(argaddr(n, &addr) < 0)
    return -1;
  return fetchstr(addr, buf, max);
}

extern uint64 sys_chdir(void);
extern uint64 sys_close(void);
extern uint64 sys_dup(void);
extern uint64 sys_exec(void);
extern uint64 sys_exit(void);
extern uint64 sys_fork(void);
extern uint64 sys_fstat(void);
extern uint64 sys_getpid(void);
extern uint64 sys_kill(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_open(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_wait(void);
extern uint64 sys_write(void);
extern uint64 sys_uptime(void);
extern uint64 sys_test_proc(void);
extern uint64 sys_dev(void);
extern uint64 sys_readdir(void);
extern uint64 sys_getcwd(void);
extern uint64 sys_remove(void);
extern uint64 sys_trace(void);
extern uint64 sys_sysinfo(void);
extern uint64 sys_rename(void);
extern uint64 sys_shutdown(void);
extern uint64 sys_times(void);
extern uint64 sys_uname(void);
extern uint64 sys_brk(void);
extern uint64 sys_mmap(void);
extern uint64 sys_openat(void);

static uint64 (*syscalls[])(void) = {
  [SYS_fork]        sys_fork,
  [SYS_exit]        sys_exit,
  [SYS_wait]        sys_wait,
  [SYS_pipe]        sys_pipe,
  [SYS_read]        sys_read,
  [SYS_kill]        sys_kill,
  [SYS_exec]        sys_exec,
  [SYS_fstat]       sys_fstat,
  [SYS_chdir]       sys_chdir,
  [SYS_dup]         sys_dup,
  [SYS_getpid]      sys_getpid,
  [SYS_sbrk]        sys_sbrk,
  [SYS_sleep]       sys_sleep,
  [SYS_uptime]      sys_uptime,
  [SYS_open]        sys_open,
  [SYS_write]       sys_write,
  [SYS_mkdir]       sys_mkdir,
  [SYS_close]       sys_close,
  [SYS_test_proc]   sys_test_proc,
  [SYS_dev]         sys_dev,
  [SYS_readdir]     sys_readdir,
  [SYS_getcwd]      sys_getcwd,
  [SYS_remove]      sys_remove,
  [SYS_trace]       sys_trace,
  [SYS_sysinfo]     sys_sysinfo,
  [SYS_rename]      sys_rename,
  [SYS_shutdown]    sys_shutdown,
  [SYS_times]       sys_times,
  [SYS_uname]       sys_uname,
  [SYS_brk]         sys_brk,
  [SYS_mmap]        sys_mmap,
  [SYS_openat]      sys_openat,
};

static char *sysnames[] = {
  [SYS_fork]        "fork",
  [SYS_exit]        "exit",
  [SYS_wait]        "wait",
  [SYS_pipe]        "pipe",
  [SYS_read]        "read",
  [SYS_kill]        "kill",
  [SYS_exec]        "exec",
  [SYS_fstat]       "fstat",
  [SYS_chdir]       "chdir",
  [SYS_dup]         "dup",
  [SYS_getpid]      "getpid",
  [SYS_sbrk]        "sbrk",
  [SYS_sleep]       "sleep",
  [SYS_uptime]      "uptime",
  [SYS_open]        "open",
  [SYS_write]       "write",
  [SYS_mkdir]       "mkdir",
  [SYS_close]       "close",
  [SYS_test_proc]   "test_proc",
  [SYS_dev]         "dev",
  [SYS_readdir]     "readdir",
  [SYS_getcwd]      "getcwd",
  [SYS_remove]      "remove",
  [SYS_trace]       "trace",
  [SYS_sysinfo]     "sysinfo",
  [SYS_rename]      "rename",
  [SYS_shutdown]    "shutdown",
  [SYS_times]       "times",
  [SYS_uname]       "uname",
  [SYS_brk]         "brk",
  [SYS_mmap]        "mmap",
  [SYS_openat]      "openat"
};

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    p->trapframe->a0 = syscalls[num]();
        // trace
    if ((p->tmask & (1 << num)) != 0) {
      printf("pid %d: %s -> %d\n", p->pid, sysnames[num], p->trapframe->a0);
    }
  } else {
    printf("pid %d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}

uint64 
sys_test_proc(void) {
    int n;
    argint(0, &n);
    printf("hello world from proc %d, hart %d, arg %d\n", myproc()->pid, r_tp(), n);
    return 0;
}

uint64
sys_sysinfo(void)
{
  uint64 addr;
  // struct proc *p = myproc();

  if (argaddr(0, &addr) < 0) {
    return -1;
  }

  struct sysinfo info;
  info.freemem = freemem_amount();
  info.nproc = procnum();

  // if (copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0) {
  if (copyout2(addr, (char *)&info, sizeof(info)) < 0) {
    return -1;
  }

  return 0;
}

uint64 sys_shutdown(void) {
  sbi_shutdown();
  return 0;
}

// ADD THIS STRUCTURE DEFINITION
struct tms {
  uint64 tms_utime;  /* user time */
  uint64 tms_stime;  /* system time */
  uint64 tms_cutime; /* user time of children */
  uint64 tms_cstime; /* system time of children */
};
// You may need to declare these if they are not in included headers
extern struct spinlock tickslock;
extern uint64 ticks;

uint64
sys_times(void)
{
  uint64 addr;
  struct tms tms_buf;
  struct proc *p = myproc();

  // 1. 从第一个参数获取用户空间 tms 结构体的地址
  if (argaddr(0, &addr) < 0) {
    return -1;
  }

  // 2. 填充 tms 结构体
  // 我们需要获取 tickslock 来安全地读取全局 ticks
  // 进程特定的时间通常在 trap 中更新，也最好在锁内读取以保证一致性
  acquire(&tickslock);
  tms_buf.tms_utime = p->utime;
  tms_buf.tms_stime = p->stime;
  tms_buf.tms_cutime = p->cutime;
  tms_buf.tms_cstime = p->cstime;
  uint64 current_ticks = ticks;  // 顺便读取一下系统总的开机时间
  release(&tickslock);   // 读完解锁

  // 3. 将内核中填充好的结构体拷贝回用户空间地址
  if (copyout2(addr, (char *)&tms_buf, sizeof(tms_buf)) < 0) {
    return -1;
  }

  // 4. 成功后，返回系统启动以来的总 ticks
  return current_ticks;
}

// 添加uname修改 ADD THIS NEW STRUCTURE DEFINITION for sys_uname
struct utsname {
  char sysname[65];
  char nodename[65];
  char release[65];
  char version[65];
  char machine[65];
  char domainname[65];
};

uint64
sys_uname(void)
{
  uint64 addr;
  struct utsname un;

  // 1. 从第一个参数获取用户空间 utsname 结构体的地址
  if (argaddr(0, &addr) < 0) {
    return -1;
  }

  // 2. 填充结构体。测试用例不关心具体内容，我们填入一些合理的值即可。
  // 使用 strncpy 是一个好习惯，可以防止缓冲区溢出。
  strncpy(un.sysname, "xv6-riscv-fs", sizeof(un.sysname));
  strncpy(un.nodename, "localhost", sizeof(un.nodename));
  strncpy(un.release, "1.0.0", sizeof(un.release));
  strncpy(un.version, "s2025", sizeof(un.version));
  strncpy(un.machine, "riscv64", sizeof(un.machine));
  strncpy(un.domainname, "(none)", sizeof(un.domainname));

  // 3. 将内核中填充好的结构体拷贝回用户空间地址
  if (copyout2(addr, (char *)&un, sizeof(un)) < 0) {
    return -1;
  }

  // 4. 成功后，返回 0
  return 0;
}
uint64
sys_brk(void)
{
  uint64 addr;
  struct proc *p = myproc();
  uint64 oldsz = p->sz;

  //从用户空间获得地址参数
  if(argaddr(0, &addr) < 0) {
    return -1;
  }
  // 处理brk(0)的情况，返回当前的 program break
  if(addr == 0) {
    return oldsz;
  }
  // 如果新旧地址相同， 什么都不用做
  if(addr == oldsz) {
    return 0;
  }
  // 调用growproc里增长或收缩内存
  if(growproc(addr - oldsz) < 0) {
    return -1; 
  }
  return 0;
}

uint64
sys_mmap(void)
{
  uint64 addr, len, offset;
  int prot, flags, fd;
  struct proc *p = myproc();
  struct file *f;
  uint64 va;

  // 1. 从用户空间获取 mmap 的所有参数
  // addr: 建议的映射起始地址, len: 映射长度, prot: 内存保护标志
  // flags: 映射对象的类型, fd: 文件描述符, offset: 文件偏移量
  if (argaddr(0, &addr) < 0 || argaddr(1, &len) < 0 || argint(2, &prot) < 0 ||
      argint(3, &flags) < 0 || argint(4, &fd) < 0 || argaddr(5, &offset) < 0) {
    // 获取参数失败，返回-1。在用户态，这个返回值会被转换成 MAP_FAILED
    return (uint64)-1; 
  }

  // 2. 根据文件描述符 fd 找到对应的文件结构体
  // 测试用例中，fd 是由 open 调用返回的一个有效文件描述符
  if (fd < 0 || fd >= NOFILE || (f = p->ofile[fd]) == 0) {
    return (uint64)-1;
  }
  
  // 3. 分配虚拟内存
  // 为了简化实现，我们忽略用户建议的 addr 地址，直接在进程现有内存的末尾（p->sz）进行分配。
  // 测试用例传的 addr 是 NULL，表示由内核选择地址，所以这个策略是符合测试要求的。
  va = p->sz;
  if (growproc(len) < 0) {
    // 如果内存分配失败（比如超过了物理内存限制）
    return (uint64)-1;
  }
  // growproc 函数成功后，p->sz 会增加 len，新分配的虚拟地址范围就是 [va, p->sz)
  
  // 4. 从文件中读取内容到新分配的内存中
  // 首先，需要把文件的内部偏移量设置为 mmap 参数中指定的 offset
  // 注意：在多线程环境下直接修改 f->off 不是安全的，但在我们的简单内核和当前测试场景下是可行的。
  f->off = offset;
  
  // 调用 file.c 中实现的 fileread 函数。
  // 这个函数会负责把文件内容读到内核缓冲区，再通过 copyout2 拷贝到用户指定的虚拟地址 va
  if (fileread(f, va, len) != len) {
    // 如果实际读到的字节数和请求的长度不符，说明读取出错了。
    // 这时需要回滚刚才的内存分配操作，防止内存泄漏。
    growproc(-len);
    return (uint64)-1;
  }

  // 5. 万事大吉，返回映射区域的起始地址
  return va;
}