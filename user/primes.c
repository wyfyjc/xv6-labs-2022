#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void createChildProcess(int fd1[2]) {
  close(fd1[1]);
  int num = 0;
  if(read(fd1[0], &num, 4) == 4) {//读取到了数字
    printf("prime %d\n", num);
  }
  else {//已无数字需要筛选
    close(fd1[0]);
    exit(0);
  }

  int prime = num;
  num = 0;
  int fd2[2];
  pipe(fd2);
  int pid = fork();
  if(pid == 0) {//子进程
    createChildProcess(fd2);//递归调用
  }
  else {//父进程
    close(fd2[0]);
    while(read(fd1[0], &num, 4) == 4) {//管道中还能读取到数字
      if(num % prime != 0) {//num不能被prime整除，可能为质数
        write(fd2[1], &num, 4);//传给子进程
      }
    }
    close(fd1[0]);
    close(fd2[1]);//问题：先close在wait，否则会造成死锁，无法正常结束
    wait(0);
  }
  exit(0);
}

int main(int argc, char *argv[])
{
  int fd[2];
  pipe(fd);
  int pid = fork();
  if(pid == 0) {//子进程
    createChildProcess(fd);
  }
  else {//父进程
    close(fd[0]);
    //逐个写入
    for(int i = 2; i <= 35; i++) {
      write(fd[1], &i, 4);
    }
    close(fd[1]);
    wait(0);//等待子进程结束
  }
  exit(0);
}