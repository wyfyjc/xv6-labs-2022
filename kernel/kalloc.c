// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];//各CPU的freelist和锁相互独立

char lockName[NCPU][7];//锁名字数组

void
kinit()
{
  for(int i=0;i<NCPU;i++) {
    snprintf(lockName[i], sizeof(lockName[i]), "kmem%d", i);//初始化锁名字
    initlock(&kmem[i].lock, lockName[i]);//初始化锁
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();

  int ID = cpuid();
  acquire(&kmem[ID].lock);
  r->next = kmem[ID].freelist;
  kmem[ID].freelist = r;
  release(&kmem[ID].lock);

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int ID = cpuid();

  acquire(&kmem[ID].lock);
  if(!kmem[ID].freelist) {//某CPU已无空闲内存
    for(int stealID = 0; stealID < NCPU; stealID++) {//依次检查其它CPU的freelist
      if(stealID != ID) {//不检查本CPU的freelist
        acquire(&kmem[stealID].lock);
        if(kmem[stealID].freelist) {//该CPU有空闲内存
          struct run *temp = kmem[stealID].freelist;//从该CPU偷取一页
          kmem[stealID].freelist = temp->next;
          temp->next = kmem[ID].freelist;
          kmem[ID].freelist = temp;
          release(&kmem[stealID].lock);//释放该CPU的kmem锁
          break;
        }
        release(&kmem[stealID].lock);//释放该CPU的kmem锁
      }
    }
  }
  r = kmem[ID].freelist;
  if(r)
    kmem[ID].freelist = r->next;
  release(&kmem[ID].lock);
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
