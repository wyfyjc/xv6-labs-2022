// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13//桶数量

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct buf bucket[NBUCKET];
  struct spinlock bucketLock[NBUCKET];
} bcache;

static inline uint hash(uint dev, uint blockno) {
    return (dev + blockno) % NBUCKET;
}

void
binit(void)
{
  initlock(&bcache.lock, "bcache_lock");//初始化全局锁

  for(int i = 0; i < NBUCKET; i++) {//初始化所有桶和桶的锁
    initlock(&bcache.bucketLock[i], "bcache_bucket_lock");
    bcache.bucket[i].next = 0;
  }

  for(int i = 0; i < NBUF; i++) {//初始化所有缓冲区
    struct buf *b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");
    b->usedTime = 0;//最后使用时间重置
    b->refcnt = 0;//引用数置0
    b->next = bcache.bucket[0].next;
    bcache.bucket[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint key = hash(dev, blockno);//计算键值

  acquire(&bcache.bucketLock[key]);//获取键值对应的桶的锁

  //在对应的哈希桶中查找该块
  for(b = bcache.bucket[key].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){//找到了对应的块
      b->refcnt++;
      release(&bcache.bucketLock[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  //不在对应的哈希桶中，则释放该桶的锁，获取搜索全局的的锁
  release(&bcache.bucketLock[key]);
  acquire(&bcache.lock);

  //在所有哈希桶中查找该块
  for(b = bcache.bucket[key].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){//找到了对应的块
      acquire(&bcache.bucketLock[key]);
      b->refcnt++;//增加一个引用数
      release(&bcache.bucketLock[key]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  //不在任何桶中，则尝试最近最少使用的缓冲区替换
  struct buf *prevBuf = 0; 
  uint holdLock = -1;
  for(int i = 0; i < NBUCKET; i++) {
    acquire(&bcache.bucketLock[i]);
    int found = 0;
    for(b = &bcache.bucket[i]; b->next; b = b->next) {
      if(b->next->refcnt == 0 && (!prevBuf || b->next->usedTime < prevBuf->next->usedTime)) {
        prevBuf = b;
        found = 1;
      }
    }
    if(!found) {//在该桶中未找到未使用的桶
      release(&bcache.bucketLock[i]);
    }
    else {
      if(holdLock != -1)//持有锁
        release(&bcache.bucketLock[holdLock]);
      holdLock = i;
    }
  }

  if(prevBuf) {//找到可替换的块，则替换
    b = prevBuf->next;
    if(holdLock != key) {//移动到新的哈希桶中
      prevBuf->next = b->next;
      release(&bcache.bucketLock[holdLock]);
      acquire(&bcache.bucketLock[key]);
      b->next = bcache.bucket[key].next;
      bcache.bucket[key].next = b;
    }
    //设定此buf的相关信息
    b->dev = dev;
    b->blockno = blockno;
    b->refcnt = 1;
    b->valid = 0;
    release(&bcache.bucketLock[key]);//释放对应桶的锁
    release(&bcache.lock);//释放全局锁
    acquiresleep(&b->lock);
    return b;
  }
  else {//所有buf均在使用中，则panic
    panic("bget: no buffers");
  }
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint key = hash(b->dev, b->blockno);

  acquire(&bcache.bucketLock[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->usedTime = ticks;
  }
  release(&bcache.bucketLock[key]);
}

void
bpin(struct buf *b) {
  uint key = hash(b->dev, b->blockno);
  acquire(&bcache.bucketLock[key]);
  b->refcnt++;
  release(&bcache.bucketLock[key]);
}

void
bunpin(struct buf *b) {
  uint key = hash(b->dev, b->blockno);
  acquire(&bcache.bucketLock[key]);
  b->refcnt--;
  release(&bcache.bucketLock[key]);
}


