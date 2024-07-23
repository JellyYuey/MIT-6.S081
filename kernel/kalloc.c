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
} kmem[NCPU];

void
kinit()
{
  char lockname[10]; // 定义一个字符数组，用于存储锁的名称
  for (int i = 0; i < NCPU; i++) // 循环遍历每个 CPU 核心
  {
    // 使用 snprintf 函数生成锁的名称，格式为 "kmem_CPU%d"
    // 并将其存储在 lockname 数组中
    snprintf(lockname, 10, "kmem_CPU%d", i);

    // 初始化 kmem[i].lock 锁，并将 lockname 作为锁的名称
    initlock(&kmem[i].lock, lockname);
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

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int cpu_id; // cpu Id

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // 获取 CPU ID 并将 r 插入对应 CPU 的空闲内存链表

  // 禁用中断，确保在获取 CPU ID 的过程中不会被打断
  push_off(); 

  // 使用 CPUID 指令获取当前 CPU 的 ID
  cpu_id = cpuid(); 

  // 重新启用中断
  pop_off(); 

  // 获取 kmem 数组中对应于当前 CPU ID 的锁，确保接下来的操作是原子性的
  acquire(&kmem[cpu_id].lock); 

  // 将 r 插入到当前 CPU 的空闲内存链表中
  r->next = kmem[cpu_id].freelist;
  kmem[cpu_id].freelist = r;

  // 释放当前 CPU 锁，允许其他操作访问空闲内存链表
  release(&kmem[cpu_id].lock); 

}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int cpu_id;

  // 禁用中断，确保在获取 CPU ID 的过程中不会被打断
  push_off();

  // 使用 CPUID 指令获取当前 CPU 的 ID
  cpu_id = cpuid(); 

  // 重新启用中断
  pop_off();

  // 获取当前 CPU 的锁，确保接下来的操作是原子性的
  acquire(&kmem[cpu_id].lock);

  // 从当前 CPU 的空闲内存链表中取出第一个元素，并将其赋值给 r
  r = kmem[cpu_id].freelist;

  // 若当前 CPU 的空闲链表有可用内存块
  if(r) {
      // 将空闲链表的头指针指向下一个内存块
      kmem[cpu_id].freelist = r->next;
  } else {
      // 若当前 CPU 无空闲内存块，尝试从其他 CPU 处“窃取”一块
      for (int i = 0; i < NCPU; i++) {
          // 跳过当前 CPU
          if (i == cpu_id) {
              continue;
          }

          // 获取其他 CPU 的锁
          acquire(&kmem[i].lock); 

          // 从其他 CPU 的空闲内存链表中取出第一个元素，并将其赋值给 r
          r = kmem[i].freelist;

          // 若找到空闲内存块
          if(r) {
              // 将该 CPU 的空闲链表头指针指向下一个内存块
              kmem[i].freelist = r->next;
          }

          // 释放其他 CPU 的锁
          release(&kmem[i].lock); 

          // 若找到空闲内存块，跳出循环
          if(r) {
              break;
          }
      }
  }

  // 释放当前 CPU 的锁
  release(&kmem[cpu_id].lock);

  // 如果成功找到空闲内存块
  if(r) {
      // 用 5 填充该内存块，作为调试标记（或 "junk" 数据）
      memset((char*)r, 5, PGSIZE); 
  }

  // 返回找到的空闲内存块，若未找到则返回 NULL
  return (void*)r;

}
