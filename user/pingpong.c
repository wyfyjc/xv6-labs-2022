#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int fd1[2], fd2[2];//子进程从fd1中读，从fd2中写，父进程相反
    pipe(fd1);
    pipe(fd2);

    int pid = fork();
    if (pid == 0) {//子进程
        close(fd1[1]);//关闭不用的读写端
        close(fd2[0]);
        char childData[2];
        read(fd1[0], &childData[0], 1);//读取
        close(fd1[0]);
        printf("%d: received ping\n", getpid());
        write(fd2[1], &childData[1], 1);//写入
        close(fd2[1]);
    }
    else { // 父进程
        close(fd2[1]);//关闭不用的读写端
        close(fd1[0]);
        char parentData[2];
        write(fd1[1], &parentData[1], 1);//写入
        close(fd1[1]);
        read(fd2[0], &parentData[0], 1);//读取
        close(fd2[0]);
        printf("%d: received pong\n", getpid());
    }
    exit(0);
}