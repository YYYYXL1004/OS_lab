//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//


#include "include/types.h"
#include "include/riscv.h"
#include "include/param.h"
#include "include/stat.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/sleeplock.h"
#include "include/file.h"
#include "include/pipe.h"
#include "include/fcntl.h"
#include "include/fat32.h"
#include "include/syscall.h"
#include "include/string.h"
#include "include/printf.h"
#include "include/vm.h"


// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == NULL)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *p = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd] == 0){
      p->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

uint64
sys_dup(void)
{
  struct file *f;
  int fd;

  // 获取旧的文件描述符
  if(argfd(0, 0, &f) < 0)
    return -1;
  
  // 分配一个新的最小可用 fd
  if((fd=fdalloc(f)) < 0)
    return -1;
  
  // 增加引用计数
  filedup(f);
  return fd;
}

// 新增 sys_dup2 
uint64
sys_dup3(void)
{
  struct file *f;
  int oldfd, newfd, flags;

  // 获取参数: oldfd(0), newfd(1), flags(2)
  if(argfd(0, &oldfd, &f) < 0) 
    return -1;
  
  if(argint(1, &newfd) < 0 || argint(2, &flags) < 0)
    return -1;

  // [关键] 检查 newfd 是否在合法范围内 (依赖 param.h 中的 NOFILE)
  if(newfd < 0 || newfd >= NOFILE)
    return -1;

  // 如果 oldfd 和 newfd 相同，直接返回 newfd，不做操作
  if(oldfd == newfd)
    return newfd;

  struct proc *p = myproc();

  // 如果 newfd 已经打开，先关闭它
  if(p->ofile[newfd]){
    struct file *f2 = p->ofile[newfd];
    p->ofile[newfd] = 0; 
    fileclose(f2);
  }

  // 执行复制
  p->ofile[newfd] = f;
  filedup(f);

  return newfd;
}
uint64
sys_read(void)
{
  struct file *f;
  int n;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;
  return fileread(f, p, n);
}

uint64
sys_write(void)
{
  struct file *f;
  int n;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;

  return filewrite(f, p, n);
}

uint64
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

uint64
sys_fstat(void)
{
  struct file *f;
  uint64 st; // user pointer to struct stat

  if(argfd(0, 0, &f) < 0 || argaddr(1, &st) < 0)
    return -1;
  return filestat(f, st);
}
// 1. 重构 create 函数，增加 base 参数
static struct dirent*
create(struct dirent *base, char *path, short type, int mode)
{
  struct dirent *ep, *dp;
  char name[FAT32_MAX_FILENAME + 1];

  // 使用 enameparent_env 代替 enameparent
  if((dp = enameparent_env(base, path, name)) == NULL)
    return NULL;

  if (type == T_DIR) {
    mode = ATTR_DIRECTORY;
  } else if (mode & O_RDONLY) {
    mode = ATTR_READ_ONLY;
  } else {
    mode = 0;  
  }

  elock(dp);
  if ((ep = ealloc(dp, name, mode)) == NULL) {
    eunlock(dp);
    eput(dp);
    return NULL;
  }
  
  if ((type == T_DIR && !(ep->attribute & ATTR_DIRECTORY)) ||
      (type == T_FILE && (ep->attribute & ATTR_DIRECTORY))) {
    eunlock(dp);
    eput(ep);
    eput(dp);
    return NULL;
  }

  eunlock(dp);
  eput(dp);

  elock(ep);
  return ep;
}
// 2. 实现 sys_openat
uint64
sys_openat(void)
{
  char path[FAT32_MAX_PATH];
  int fd, omode, dirfd;
  struct file *f;
  struct dirent *ep;
  struct dirent *base = NULL; // 默认为 NULL，表示相对于 CWD

  // 获取参数：dirfd(0), path(1), omode(2)
  if(argint(0, &dirfd) < 0 || argstr(1, path, FAT32_MAX_PATH) < 0 || argint(2, &omode) < 0)
    return -1;

  // 确定查找的基准目录 (base)
  if (path[0] != '/' && dirfd != AT_FDCWD) {
      // 如果是相对路径，且 dirfd 不是 AT_FDCWD，则需要从 dirfd 获取 base
      if (dirfd < 0 || dirfd >= NOFILE || (f = myproc()->ofile[dirfd]) == 0 || f->type != FD_ENTRY) {
          return -1; // 无效的 dirfd
      }
      base = f->ep; 
  }

  if(omode & O_CREATE){
    // 调用带 base 的 create
    ep = create(base, path, T_FILE, omode);
    if(ep == NULL){
      return -1;
    }
  } else {
    // 调用带 base 的 ename
    if((ep = ename_env(base, path)) == NULL){
      return -1;
    }
    elock(ep);
    // 1. 如果指定了 O_DIRECTORY，但打开的不是目录，则报错
    if((omode & O_DIRECTORY) && !(ep->attribute & ATTR_DIRECTORY)){
      eunlock(ep);
      eput(ep);
      return -1;
    }

    // 2. 如果是目录，且使用了 写权限，则报错
    if((ep->attribute & ATTR_DIRECTORY) && (omode & (O_WRONLY|O_RDWR))){
      eunlock(ep);
      eput(ep);
      return -1;
    }
  }

  if((f = filealloc()) == NULL || (fd = fdalloc(f)) < 0){
    if (f) {
      fileclose(f);
    }
    eunlock(ep);
    eput(ep);
    return -1;
  }

  if(!(ep->attribute & ATTR_DIRECTORY) && (omode & O_TRUNC)){
    etrunc(ep);
  }

  f->type = FD_ENTRY;
  f->off = (omode & O_APPEND) ? ep->file_size : 0;
  f->ep = ep;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  eunlock(ep);

  return fd;
}

// 3. 更新 sys_open，使其兼容新的 create 接口
uint64
sys_open(void)
{
  char path[FAT32_MAX_PATH];
  int fd, omode;
  struct file *f;
  struct dirent *ep;

  if(argstr(0, path, FAT32_MAX_PATH) < 0 || argint(1, &omode) < 0)
    return -1;

  if(omode & O_CREATE){
    // 传入 NULL 作为 base，表示使用 CWD
    ep = create(NULL, path, T_FILE, omode);
    if(ep == NULL){
      return -1;
    }
  } else {
    if((ep = ename(path)) == NULL){
      return -1;
    }
    elock(ep);
    // 1. 如果指定了 O_DIRECTORY，但打开的不是目录，则报错
    if((omode & O_DIRECTORY) && !(ep->attribute & ATTR_DIRECTORY)){
      eunlock(ep);
      eput(ep);
      return -1;
    }

    // 2. 如果是目录，且使用了 写权限，则报错
    if((ep->attribute & ATTR_DIRECTORY) && (omode & (O_WRONLY|O_RDWR))){
      eunlock(ep);
      eput(ep);
      return -1;
    }
  }

  if((f = filealloc()) == NULL || (fd = fdalloc(f)) < 0){
    if (f) {
      fileclose(f);
    }
    eunlock(ep);
    eput(ep);
    return -1;
  }

  if(!(ep->attribute & ATTR_DIRECTORY) && (omode & O_TRUNC)){
    etrunc(ep);
  }

  f->type = FD_ENTRY;
  f->off = (omode & O_APPEND) ? ep->file_size : 0;
  f->ep = ep;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  eunlock(ep);

  return fd;
}

// 4. 更新 sys_mkdir，因为 create 的签名变了
uint64
sys_mkdir(void)
{
  char path[FAT32_MAX_PATH];
  struct dirent *ep;

  // 传入 NULL 作为 base
  if(argstr(0, path, FAT32_MAX_PATH) < 0 || (ep = create(NULL, path, T_DIR, 0)) == 0){
    return -1;
  }
  eunlock(ep);
  eput(ep);
  return 0;
}

uint64
sys_chdir(void)
{
  char path[FAT32_MAX_PATH];
  struct dirent *ep;
  struct proc *p = myproc();
  
  if(argstr(0, path, FAT32_MAX_PATH) < 0 || (ep = ename(path)) == NULL){
    return -1;
  }
  elock(ep);
  if(!(ep->attribute & ATTR_DIRECTORY)){
    eunlock(ep);
    eput(ep);
    return -1;
  }
  eunlock(ep);
  eput(p->cwd);
  p->cwd = ep;
  return 0;
}

uint64
sys_pipe(void)
{
  uint64 fdarray; // user pointer to array of two integers
  struct file *rf, *wf;
  int fd0, fd1;
  struct proc *p = myproc();

  if(argaddr(0, &fdarray) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      p->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  // if(copyout(p->pagetable, fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
  //    copyout(p->pagetable, fdarray+sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0){
  if(copyout2(fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
     copyout2(fdarray+sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0){
    p->ofile[fd0] = 0;
    p->ofile[fd1] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  return 0;
}

// To open console device.
uint64
sys_dev(void)
{
  int fd, omode;
  int major, minor;
  struct file *f;

  if(argint(0, &omode) < 0 || argint(1, &major) < 0 || argint(2, &minor) < 0){
    return -1;
  }

  if(omode & O_CREATE){
    panic("dev file on FAT");
  }

  if(major < 0 || major >= NDEV)
    return -1;

  if((f = filealloc()) == NULL || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    return -1;
  }

  f->type = FD_DEVICE;
  f->off = 0;
  f->ep = 0;
  f->major = major;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  return fd;
}

// To support ls command
uint64
sys_readdir(void)
{
  struct file *f;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argaddr(1, &p) < 0)
    return -1;
  return dirnext(f, p);
}

// get absolute cwd string
uint64
sys_getcwd(void)
{
  uint64 addr;
  int size;

  // 1. 同时获取 buf 地址和 buf 大小两个参数
  if (argaddr(0, &addr) < 0 || argint(1, &size) < 0)
    return 0; // 参数获取失败

  // 2. 校验参数合法性
  if (addr == 0 || size <= 0)
    return 0; // 无效的缓冲区或大小

  struct dirent *de = myproc()->cwd;
  char path[FAT32_MAX_PATH];
  char *s;
  int len;

  if (de->parent == NULL) {  // 如果当前就是根目录
    s = "/";
  } else {
    s = path + FAT32_MAX_PATH - 1; //指针s先指向临时缓冲区path的末尾
    *s = '\0';  // 在最末尾放一个字符串结束符
    // 循环向上找父目录
    while (de->parent) {
      len = strlen(de->filename);
      s -= len;               // 指针s向前移动 目录名的长度
      if (s <= path)          // 路径过长，超出了内核的临时缓冲区
        return 0;
      strncpy(s, de->filename, len);  // 拷贝目录名
      *--s = '/';           // 指针再向前移动一位，放一个'/'
      de = de->parent;      // 切换到父目录，准备下一次循环
    }
  }
  
  // 3. 检查路径长度是否会超出用户缓冲区
  int n = strlen(s) + 1; // +1 为了包含结尾的 '\0'
  if (n > size) {
    return 0; // 用户缓冲区太小
  }
  
  // 4. 拷贝到用户空间
  if (copyout2(addr, s, n) < 0)
    return 0; // 拷贝失败

  // 5. 成功时返回用户缓冲区的地址
  return addr;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct dirent *dp)
{
  struct dirent ep;
  int count;
  int ret;
  ep.valid = 0;
  ret = enext(dp, &ep, 2 * 32, &count);   // skip the "." and ".."
  return ret == -1;
}

uint64
sys_remove(void)
{
  char path[FAT32_MAX_PATH];
  struct dirent *ep;
  int len;
  if((len = argstr(0, path, FAT32_MAX_PATH)) <= 0)
    return -1;

  char *s = path + len - 1;
  while (s >= path && *s == '/') {
    s--;
  }
  if (s >= path && *s == '.' && (s == path || *--s == '/')) {
    return -1;
  }
  
  if((ep = ename(path)) == NULL){
    return -1;
  }
  elock(ep);
  if((ep->attribute & ATTR_DIRECTORY) && !isdirempty(ep)){
      eunlock(ep);
      eput(ep);
      return -1;
  }
  elock(ep->parent);      // Will this lead to deadlock?
  eremove(ep);
  eunlock(ep->parent);
  eunlock(ep);
  eput(ep);

  return 0;
}

// Must hold too many locks at a time! It's possible to raise a deadlock.
// Because this op takes some steps, we can't promise
uint64
sys_rename(void)
{
  char old[FAT32_MAX_PATH], new[FAT32_MAX_PATH];
  if (argstr(0, old, FAT32_MAX_PATH) < 0 || argstr(1, new, FAT32_MAX_PATH) < 0) {
      return -1;
  }

  struct dirent *src = NULL, *dst = NULL, *pdst = NULL;
  int srclock = 0;
  char *name;
  if ((src = ename(old)) == NULL || (pdst = enameparent(new, old)) == NULL
      || (name = formatname(old)) == NULL) {
    goto fail;          // src doesn't exist || dst parent doesn't exist || illegal new name
  }
  for (struct dirent *ep = pdst; ep != NULL; ep = ep->parent) {
    if (ep == src) {    // In what universe can we move a directory into its child?
      goto fail;
    }
  }

  uint off;
  elock(src);     // must hold child's lock before acquiring parent's, because we do so in other similar cases
  srclock = 1;
  elock(pdst);
  dst = dirlookup(pdst, name, &off);
  if (dst != NULL) {
    eunlock(pdst);
    if (src == dst) {
      goto fail;
    } else if (src->attribute & dst->attribute & ATTR_DIRECTORY) {
      elock(dst);
      if (!isdirempty(dst)) {    // it's ok to overwrite an empty dir
        eunlock(dst);
        goto fail;
      }
      elock(pdst);
    } else {                    // src is not a dir || dst exists and is not an dir
      goto fail;
    }
  }

  if (dst) {
    eremove(dst);
    eunlock(dst);
  }
  memmove(src->filename, name, FAT32_MAX_FILENAME);
  emake(pdst, src, off);
  if (src->parent != pdst) {
    eunlock(pdst);
    elock(src->parent);
  }
  eremove(src);
  eunlock(src->parent);
  struct dirent *psrc = src->parent;  // src must not be root, or it won't pass the for-loop test
  src->parent = edup(pdst);
  src->off = off;
  src->valid = 1;
  eunlock(src);

  eput(psrc);
  if (dst) {
    eput(dst);
  }
  eput(pdst);
  eput(src);

  return 0;

fail:
  if (srclock)
    eunlock(src);
  if (dst)
    eput(dst);
  if (pdst)
    eput(pdst);
  if (src)
    eput(src);
  return -1;
}

uint64
sys_pipe2(void)
{
  uint64 fdarray; // 用户空间的数组指针: int fd[2]
  int flags;      // pipe2 的第二个参数 flags
  struct file *rf, *wf;
  int fd0, fd1;
  struct proc *p = myproc();

  // 获取参数：pipe2(int pipefd[2], int flags)
  // 参数0: fdarray指针, 参数1: flags
  if(argaddr(0, &fdarray) < 0 || argint(1, &flags) < 0)
    return -1;

  if(pipealloc(&rf, &wf) < 0)
    return -1;

  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      p->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }

  // 将生成的文件描述符写回用户空间
  if(copyout2(fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
     copyout2(fdarray+sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0){
    p->ofile[fd0] = 0;
    p->ofile[fd1] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  return 0;
}

// kernel/sysfile.c

// 定义 linux_dirent64 结构体和文件类型常量
struct linux_dirent64 {
  uint64        d_ino;    // 索引节点号
  uint64         d_off;    // 到下一个 dirent 的偏移量
  unsigned short d_reclen; // 当前 dirent 的长度
  unsigned char  d_type;   // 文件类型
  char           d_name[]; // 文件名
};

#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10
#define DT_SOCK    12
#define DT_WHT     14

uint64
sys_getdents64(void)
{
  struct file *f;
  int fd;
  uint64 buf;    // 用户缓冲区地址
  int len;       // 用户缓冲区长度

  // 获取参数: getdents64(int fd, struct linux_dirent64 *dirp, unsigned int count)
  if(argfd(0, &fd, &f) < 0 || argaddr(1, &buf) < 0 || argint(2, &len) < 0)
    return -1;

  // 检查是否为目录且可读
  if(f->readable == 0 || !(f->ep->attribute & ATTR_DIRECTORY))
    return -1;

  int count = 0; // enext 返回的 entry 数量 (包含 LFN)
  int ret;
  struct dirent de;
  int nread = 0;
  
  elock(f->ep);

  while(1) {
    // 循环调用 enext 跳过空闲的目录项，直到找到有效文件或结束
    // enext 会从 f->off 读取，并在内部处理
    // 注意：我们需要自己根据 enext 返回的 count 更新 f->off
    
    while((ret = enext(f->ep, &de, f->off, &count)) == 0) {
      // ret == 0 表示遇到空槽位 (empty slot)，跳过
      f->off += count * 32;
    }
    
    if(ret == -1) {
      // 目录遍历结束
      break; 
    }

    // 此时 de 中包含了有效的文件信息 (文件名等)
    int name_len = strlen(de.filename);
    // 计算当前 dirent 需要的大小: 头部固定长度 + 文件名长度 + 1(null terminator)
    int reclen = 19 + name_len + 1; // 19 = 8(d_ino) + 8(d_off) + 2(d_reclen) + 1(d_type)
    reclen = (reclen + 7) & ~7;     // 向上对齐到 8 字节边界

    // 检查用户缓冲区是否足够
    if(nread + reclen > len) {
      if(nread == 0) {
          // 连第一个都放不下，返回错误或 0? 通常返回 -1 或 EINVAL
          eunlock(f->ep);
          return -1;
      }
      // 缓冲区满了，下次再读
      break; 
    }

    struct linux_dirent64 lde;
    lde.d_ino = 0; // FAT32 没有 inode，置 0
    // d_off 应该是指向下一个 dirent 的偏移量
    lde.d_off = f->off + count * 32; 
    lde.d_reclen = reclen;
    lde.d_type = (de.attribute & ATTR_DIRECTORY) ? DT_DIR : DT_REG;

    // 1. 拷贝结构体头部 (19字节)
    if(copyout2(buf, (char*)&lde, 19) < 0) {
      eunlock(f->ep);
      return -1;
    }
    // 2. 拷贝文件名
    if(copyout2(buf + 19, de.filename, name_len + 1) < 0) {
      eunlock(f->ep);
      return -1;
    }
    
    // 如果需要清零填充字节，可以在这里处理，但非必须

    // 更新状态
    buf += reclen;
    nread += reclen;
    len -= reclen;
    f->off += count * 32; // 更新文件偏移量，准备读取下一个
  }

  eunlock(f->ep);
  return nread; // 返回读取的总字节数
}

uint64
sys_mkdirat(void)
{
  int dirfd;
  char path[FAT32_MAX_PATH];
  int mode;
  struct dirent *ep;
  struct file *f;
  struct dirent *base = NULL; // 默认为 NULL，表示相对于 CWD

  // 获取参数: mkdirat(int dirfd, const char *pathname, mode_t mode)
  if(argint(0, &dirfd) < 0 || argstr(1, path, FAT32_MAX_PATH) < 0 || argint(2, &mode) < 0)
    return -1;

  // 确定查找基准 (base)
  // 如果路径是相对路径 且 dirfd 不是 AT_FDCWD
  if(path[0] != '/' && dirfd != AT_FDCWD){
    if(dirfd < 0 || dirfd >= NOFILE || (f = myproc()->ofile[dirfd]) == 0 || f->type != FD_ENTRY){
      return -1; // 无效的文件描述符
    }
    base = f->ep;
  }

  // 调用 create 创建目录 (T_DIR)
  if((ep = create(base, path, T_DIR, mode)) == 0){
    return -1;
  }
  
  eunlock(ep);
  eput(ep);
  return 0;
}

uint64
sys_unlinkat(void)
{
  int dirfd;
  char path[FAT32_MAX_PATH];
  int flags;
  struct dirent *ep;
  struct file *f;
  struct dirent *base = NULL;
  
  // 获取参数: unlinkat(int dirfd, const char *pathname, int flags)
  if(argint(0, &dirfd) < 0 || argstr(1, path, FAT32_MAX_PATH) < 0 || argint(2, &flags) < 0)
    return -1;

  // 确定查找基准
  if(path[0] != '/' && dirfd != AT_FDCWD){
    if(dirfd < 0 || dirfd >= NOFILE || (f = myproc()->ofile[dirfd]) == 0 || f->type != FD_ENTRY){
      return -1;
    }
    base = f->ep;
  }

  // 查找目标文件/目录
  // 注意：unlink 测试用例可能删除名为 "." 的特殊路径，需要特判或由 ename 处理
  char name[FAT32_MAX_FILENAME + 1];
  // 使用 enameparent_env 找到父目录和文件名，这样方便加锁删除
  struct dirent *dp = enameparent_env(base, path, name);
  if(!dp) return -1;

  elock(dp); // 锁住父目录

  // 在父目录中查找目标
  uint off;
  ep = dirlookup(dp, name, &off);
  if(!ep){
      eunlock(dp);
      eput(dp);
      return -1;
  }
  
  elock(ep); // 锁住目标文件

  // 检查是否是目录
  if(ep->attribute & ATTR_DIRECTORY){
      // 如果是目录，且没有设置 AT_REMOVEDIR 标志，则 unlink 应该失败
      // (但在简单的 FAT32 实现中，为了通过某些宽泛的测试，有时会放宽此限制。
      //  标准行为是：unlink不能删目录，rmdir(即flags & AT_REMOVEDIR)才能删)
      if((flags & AT_REMOVEDIR) == 0){
          eunlock(ep);
          eput(ep);
          eunlock(dp);
          eput(dp);
          return -1; // Is a directory
      }
      // 如果是目录，必须为空才能删除
      if(!isdirempty(ep)){
          eunlock(ep);
          eput(ep);
          eunlock(dp);
          eput(dp);
          return -1; // Not empty
      }
  }

  // 执行删除操作
  eremove(ep);
  
  eunlock(ep);
  eput(ep);
  eunlock(dp);
  eput(dp);

  return 0;
}