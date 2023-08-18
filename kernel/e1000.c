#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *tx_mbufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
void
e1000_init(uint32 *xregs)
{
  int i;

  initlock(&e1000_lock, "e1000");

  regs = xregs;

  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_mbufs[i] = 0;
  }
  regs[E1000_TDBAL] = (uint64) tx_ring;
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;
  
  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head;
  }
  regs[E1000_RDBAL] = (uint64) rx_ring;
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  regs[E1000_RDLEN] = sizeof(rx_ring);

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back
}

int
e1000_transmit(struct mbuf *m)
{
  //
  // Your code here.
  //
  // the mbuf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after sending.
  //
  acquire(&e1000_lock);//为e1000加锁，防止多进程同时发送数据导致出错

  uint32 TXBufferIndex = regs[E1000_TDT];//获取TX缓冲区描述符索引
  struct tx_desc *TXDescriptor = &tx_ring[TXBufferIndex];//获取TX缓冲区的描述符
  
  if(!(TXDescriptor->status & E1000_TXD_STAT_DD)) {//缓冲区中的数据尚未传输完成，返回错误
    release(&e1000_lock);
    return -1;
  }

  if(tx_mbufs[TXBufferIndex])//释放已完成传输的mbuf
    mbuffree(tx_mbufs[TXBufferIndex]);
  
  TXDescriptor->addr = (uint64)m->head;//待发送数据的地址
  TXDescriptor->length = m->len;//待发送数据的长度
  TXDescriptor->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;//设置cmd字段，表示该数据是一个完整的数据包，且发送完成后需要将E1000_TXD_STAT_DD置1
  regs[E1000_TDT] = (regs[E1000_TDT] + 1) % TX_RING_SIZE;//切换到环形缓冲区中的下一个位置
  
  release(&e1000_lock);//解锁
  return 0;
}

static void
e1000_recv(void)
{
  //
  // Your code here.
  //
  // Check for packets that have arrived from the e1000
  // Create and deliver an mbuf for each packet (using net_rx()).
  //
  while(1) {
    uint32 RXBufferIndex = (regs[E1000_RDT] + 1) % RX_RING_SIZE;//获取RX缓冲区描述符索引
    struct rx_desc *RXDescriptor = &rx_ring[RXBufferIndex];//获取RX缓冲区的描述符
    
    if(!(RXDescriptor->status & E1000_RXD_STAT_DD))//已接收完毕
      return;

    rx_mbufs[RXBufferIndex]->len = RXDescriptor->length;//待接收数据的长度
    net_rx(rx_mbufs[RXBufferIndex]);//将mbuf传递给网络栈
    rx_mbufs[RXBufferIndex] = mbufalloc(0);//分配新mbuf
    RXDescriptor->addr = (uint64)rx_mbufs[RXBufferIndex]->head;//记录新地址
    RXDescriptor->status = 0;//状态位置0
    regs[E1000_RDT] = RXBufferIndex;//更新寄存器中的RX缓冲区描述符索引
  }
}

void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}
