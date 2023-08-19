#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if(argc == 1) {
        printf("请输入睡眠的时间");
    }
    else {
        int sleepTime = atoi(argv[1]);
        sleep(sleepTime);
    }
    exit(0);
}