struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  uint usedTime;//最近使用时间
  struct buf *next;
  uchar data[BSIZE];
};

