#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define READEND 0  // 定义常量，表示管道的读取端
#define WRITEEND 1 // 定义常量，表示管道的写入端

void makeChild(int *pleft); // 函数声明，用于创建子进程并处理管道数据

int main(int argc, char *argv[])
{
    int p[2]; // 创建一个长度为2的整型数组，用于存储管道的文件描述符
    pipe(p);  // 创建管道

    if (fork() == 0) // 创建子进程，如果当前进程是子进程
    {
        makeChild(p); // 子进程调用makeChild函数处理管道数据
    }
    else // 如果当前进程是父进程
    {
        close(p[READEND]); // 关闭父进程中管道的读取端
        for (int i = 2; i <= 35; i++) // 遍历2到35的整数
        {
            write(p[WRITEEND], &i, sizeof(int)); // 将每个整数写入管道
        }
        close(p[WRITEEND]); // 关闭父进程中管道的写入端
        wait((int *)0); // 等待子进程结束
    }
    exit(0); // 退出程序
    return 0;
}

void makeChild(int *pleft)
{
    int pright[2]; // 创建一个长度为2的整型数组，用于存储右侧管道的文件描述符
    int first; // 用于存储从管道读取的第一个整数
    close(pleft[WRITEEND]); // 关闭左侧管道的写入端

    int isReadEnd = read(pleft[READEND], &first, sizeof(int)); // 从左侧管道读取第一个整数
    if (isReadEnd == 0) // 如果读取到的值为0，表示到达终点
    {
        exit(0); // 退出当前进程
    }
    else // 如果读取到的值不是0
    {
        printf("prime %d \n", first); // 打印读取到的第一个整数，表示一个素数
        pipe(pright); // 创建右侧管道
        if (fork() == 0) // 创建子进程，如果当前进程是子进程
        {
            makeChild(pright); // 子进程调用makeChild函数处理右侧管道数据
        }
        else // 如果当前进程是父进程
        {
            for (;;) // 无限循环处理左侧管道的数据
            {
                int num; // 用于存储从管道读取的整数
                close(pright[READEND]); // 关闭右侧管道的读取端
                if (read(pleft[READEND], &num, sizeof(int)) == 0) // 从左侧管道读取数据
                    break; // 如果读取到的数据为0，跳出循环
                else // 如果读取到的数据不是0
                {
                    if (num % first != 0) // 如果读取到的整数不是first的倍数
                    {
                        write(pright[WRITEEND], &num, sizeof(int)); // 将整数写入右侧管道
                    }
                }
            }
            close(pright[WRITEEND]); // 关闭右侧管道的写入端
            close(pleft[READEND]); // 关闭左侧管道的读取端
            wait((int *)0); // 等待子进程结束
        }
    }
    exit(0); // 退出当前进程
}
