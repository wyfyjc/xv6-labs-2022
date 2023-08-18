#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

//修改（sysinfo）
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

//修改（trace）
uint64
sys_trace(void)
{
  int mask;
  argint(0, &mask);
  struct proc *p = myproc();
  p->mask = mask;
  return 0;
}

//修改（sysinfo）
uint64
sys_sysinfo(void)
{
  uint64 address;
  argaddr(0, &address);
  int process;
  int freeMemory;
  process = countProcess();
  freeMemory = countFreeMemory();
  struct sysinfo sysinfo;
  sysinfo.freemem = freeMemory;
  sysinfo.nproc = process;

  struct proc *p = myproc();
  if (copyout(p->pagetable, address, (char *)&sysinfo, sizeof(sysinfo)) < 0)
    return -1;

  return 0;
}