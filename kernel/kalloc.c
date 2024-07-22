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
} kmem;
//(PHYSTOP - KERNBASE) / PGSIZE 计算的是内核所覆盖的内存页数
//定义了一个 reference_count 数组,用于存储每个内存页的引用计数S
uint64 reference_count[(PHYSTOP-KERNBASE)/PGSIZE];

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  //将 reference_count 数组的所有元素初始化为 0
  memset(reference_count, 0, sizeof(reference_count));
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    k_incre_ref(p);//增加当前页面的引用计数
    kfree(p);//释放当前页面的内存
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  //计算索引
  uint64 index = ((uint64)pa-PGROUNDUP((uint64)end)) / PGSIZE;
  //减少引用计数
  //如果减少后的引用计数不为零，则直接返回
  if(--reference_count[index] != 0)
    return;

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;//将 r 从空闲页面链表中移除，并将链表头指针指向下一个空闲页面
    // pa0 = walkaddr(pagetable, va0);
    //计算物理页面的索引
    uint64 index = (PGROUNDUP((uint64)r)-PGROUNDUP((uint64)end)) / PGSIZE;
    reference_count[index] = 1;//将计算出的索引对应的引用计数设置为 1
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

void
k_incre_ref(void* pa)
{
  uint64 index = (PGROUNDUP((uint64)pa)-PGROUNDUP((uint64)end)) / PGSIZE;
  reference_count[index]++;
}