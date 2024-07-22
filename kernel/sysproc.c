#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
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
  backtrace();
  return 0;
}

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
// lab4-3
// 定义系统调用 sys_sigalarm，用于设置一个间隔时间和信号处理程序
uint64 sys_sigalarm(void) {
    int interval; // 存储间隔时间
    uint64 handler; // 存储信号处理程序的地址
    struct proc *p; // 指向当前进程的指针

    // 获取系统调用的参数，如果参数无效或间隔时间为负数，则返回 -1 表示错误
    if (argint(0, &interval) < 0 || argaddr(1, &handler) < 0 || interval < 0) {
        return -1;
    }

    // 获取当前进程的指针
    p = myproc();

    // 设置当前进程的信号处理程序间隔时间
    p->interval = interval;

    // 设置当前进程的信号处理程序地址
    p->handler = handler;

    // 重置过去的时钟数为 0
    p->passedticks = 0;

    // 成功返回 0
    return 0;
}


// lab4-3
uint64 sys_sigreturn(void) {
    struct proc* p = myproc();  // 获取当前进程的指针

    // trapframecopy 必须是 trapframe 的副本，确保其位置在 trapframe 之后的 512 字节处
    if(p->trapframecopy != p->trapframe + 512) {
        return -1;  // 如果位置不正确，返回错误
    }

    // 将保存的 trapframe 副本恢复到当前 trapframe 中
    memmove(p->trapframe, p->trapframecopy, sizeof(struct trapframe));   // 恢复 trapframe

    p->passedticks = 0;  // 重置已过去的时钟数，防止重入
    p->trapframecopy = 0;  // 将 trapframecopy 置零

    return p->trapframe->a0;  // 返回 a0 寄存器的值，以避免返回值被覆盖
}



