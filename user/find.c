#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char *path, char *name) {//大部分代码和ls相同
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_DEVICE:
  case T_FILE:
    printf("%s is not a path\n", path);
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("find: cannot stat %s\n", buf);
        continue;
      }

      //修改begin
      switch(st.type){
        case T_DEVICE:
        case T_FILE: //是文件
          if(strcmp(de.name, name) == 0) {//是要找的文件
            printf("%s\n", buf);
          }
          break;
        case T_DIR: //是目录
          //忽略.和..两个目录
          if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
            break;
          }
          find(buf, name);//对此目录进行递归查找
      }
      //修改end
    }
    break;
  }
  close(fd);
}

int main(int argc, char *argv[])
{
  if(argc != 3) {
    printf("输入错误\n");
    exit(0);
  }

  find(argv[1], argv[2]);
  exit(0);
}