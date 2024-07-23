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
struct spinlock e1000_txlock;
struct spinlock e1000_rxlock;

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

//传输数据包的函数
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
  
  acquire(&e1000_txlock);
  //读取 E1000_TDT 控制寄存器，向E1000询问等待下一个数据包的TX环索引
    uint32 tail = regs[E1000_TDT];
    //检查环是否溢出
    if(!(tx_ring[tail].status & E1000_TXD_STAT_DD)){
    //如果 E1000_TXD_STAT_DD 未在 E1000_TDT 索引的描述符中设置
    //则E1000尚未完成先前相应的传输请求
    //返回错误
        release(&e1000_txlock);
        return -1;
    }
    //使用 mbuffree() 释放从该描述符传输的最后一个 mbuf （如果有）
    if(tx_mbufs[tail])
        mbuffree(tx_mbufs[tail]);
    //填写描述符
    //m->head 指向内存中数据包的内容
    tx_ring[tail].addr = (uint64)m->head;
    //m->len 是数据包的长度
    tx_ring[tail].length = (uint16)m->len;
    //设置必要的cmd标志
    tx_ring[tail].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
    //保存指向 mbuf 的指针，以便稍后释放
    tx_mbufs[tail] = m;
    //更新环位置
    regs[E1000_TDT] = (tail+1) % TX_RING_SIZE;
    release(&e1000_txlock);
    return 0;
}

//接受数据的函数
static void
e1000_recv(void)
{
  //
  // Your code here.
  //
  // Check for packets that have arrived from the e1000
  // Create and deliver an mbuf for each packet (using net_rx()).
  //
  struct mbuf *newmbuf;
    acquire(&e1000_rxlock);
    //提取 E1000_RDT 控制寄存器并加一对 RX_RING_SIZE 取模
    //向E1000询问下一个等待接收数据包（如果有）所在的环索引
    uint32 tail = regs[E1000_RDT];
    uint32 curr = (tail + 1) % RX_RING_SIZE;
    while(1){
      //过检查描述符 status 部分中的 E1000_RXD_STAT_DD 位来检查新数据包是否可用
        if(!(rx_ring[curr].status & E1000_RXD_STAT_DD)){
            break;
        }
        //将 mbuf 的 m->len 更新为描述符中报告的长度
        rx_mbufs[curr]->len = rx_ring[curr].length;
        net_rx(rx_mbufs[curr]);
        tail = curr;
        //使用 mbufalloc() 分配一个新的 mbuf ，以替换刚刚给 net_rx() 的 mbuf
        newmbuf = mbufalloc(0);
        rx_mbufs[curr] = newmbuf;
        rx_ring[curr].addr = (uint64)newmbuf->head;
        rx_ring[curr].status = 0;
        regs[E1000_RDT] = curr;
        curr = (curr + 1) % RX_RING_SIZE;
    }
    //将 E1000_RDT 寄存器更新为最后处理的环描述符的索引
    regs[E1000_RDT] = tail;
    release(&e1000_rxlock);
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
