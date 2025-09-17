// init: The initial user-level program

#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "kernel/include/file.h"
#include "kernel/include/fcntl.h"
#include "xv6-user/user.h"

// 重构代码，打破无限循环
// 1. 在这里定义所有需要按顺序执行的测试程序
char *tests[] = {
  "sh",
  // "test2", // 将来可以像这样添加新的测试
  // "test3"
};

char *argv[] = { 0, 0 }; // 准备一个空的 argv

int
main(void)
{
  int pid, wpid;

  dev(O_RDWR, CONSOLE, 0);
  dup(0);  // stdout
  dup(0);  // stderr

  // 2. 将无限循环改为遍历 tests 数组的有限循环
  for(int i = 0; i < sizeof(tests)/sizeof(tests[0]); i++){
    printf("init: starting test [%s]\n", tests[i]);
    pid = fork();
    if(pid < 0){
      printf("init: fork failed\n");
      exit(1);
    }
    if(pid == 0){
      // 子进程：执行当前测试程序
      argv[0] = tests[i]; // 将程序名作为 argv[0]
      exec(tests[i], argv);
      printf("init: exec %s failed\n", tests[i]);
      exit(1);
    }

    // 父进程 (init) 等待当前测试程序结束
    while((wpid=wait(0)) >= 0 && wpid != pid)
      printf("zombie!\n");
  }

  // 3. 所有测试都已执行完毕，现在调用关机
  printf("init: all tests finished. Shutting down...\n");
  shutdown();
  
  exit(0);
}
