#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_APPEND  0x008  // 追加模式
#define O_CREATE  0x40
#define O_DIRECTORY 0x0200000
#define O_TRUNC   0x400  // 截断文件
#define O_NOFOLLOW 0x20000
#define O_CLOEXEC 0x80000

// 添加 AT_FDCWD 定义，通常为 -100
// 告诉内核，如果提供的路径是相对路径，请相对于当前工作目录（Current Working Directory）来查找，
// 而不是相对于某个特定的文件描述符。
#define AT_FDCWD  -100