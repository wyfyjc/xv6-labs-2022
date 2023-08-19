#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[])
{
  char *args[MAXARG];//存储所有参数

  if(argc == 1) {//无需要执行的命令
    exit(0);
  }
  else {//有需要执行的命令
    for(int i = 1; i < argc; i++) {//添加xargs中已经带有的参数
      args[i - 1] = argv[i];
    }
    int loop = 1;
    while(loop) {//为每行执行一次
      char buf[512];//存储附加参数
      for(int i = 0; i < 511; i++) {//添加附加参数
        if(read(0, buf + i, 1) == 0) {//读到了EOF
          loop = 0;//终止循环
          break;
        }
        if(buf[i] == '\n') {//读取到了换行符
          buf[i] = '\0';//删除换行符
          args[argc - 1] = buf;//将buf作为最后一个参数
          int pid = fork();//创建子进程
          if(pid == 0) {//子进程执行命令
            exec(args[0], args);
          }
          else {//父进程等待
            wait(0);
            args[argc - 1] = 0;//清空附加参数
          }
          break;
        }
      }
    }
  }
  exit(0);
}