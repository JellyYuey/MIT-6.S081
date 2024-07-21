#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
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
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
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


  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int sys_pgaccess(void)
{
  uint64 addr;    // 用户传入的虚拟地址
  int len;        // 要检查的页数
  int bitmask;    // 用于存储结果的位掩码

  // 获取用户传入的第一个参数，即虚拟地址
  if(argaddr(0, &addr) < 0){
    return -1;
  }

  // 获取用户传入的第二个参数，即要检查的页数
  if(argint(1, &len) < 0){
    return -1;
  }

  // 获取用户传入的第三个参数，即位掩码的地址
  if(argint(2, &bitmask) < 0){
    return -1;
  }

  // 检查页数是否在有效范围内 (0 到 32)
  if(len > 32 || len < 0){
    return -1;
  }

  int res = 0;  // 用于存储访问结果
  struct proc *p = myproc();  // 获取当前进程

  // 遍历每一页并检查访问位
  for(int i = 0; i < len; i++){
    int va = addr + i * PGSIZE;  // 计算虚拟地址
    int abit = vm_pgaccess(p->pagetable, va);  // 检查访问位
    res = res | (abit << i);  // 将结果存储在res中，对应页的结果存储在对应位置
  }

  // 将结果从内核空间复制到用户空间
  if(copyout(p->pagetable, bitmask, (char*)&res, sizeof(res)) < 0){
    return -1;
  }

  return 0;
}

#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
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