#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    // 创建管道会得到一个长度为2的int数组
    // 其中0为用于从管道读取数据的文件描述符，1为用于向管道写入数据的文件描述符
    int ping[2], pong[2];
    pipe(ping); // 创建用于父进程 -> 子进程的管道
    pipe(pong); // 创建用于子进程 -> 父进程的管道

    if(fork() != 0) { // 父进程
        close(ping[0]);  // 关闭不必要的文件描述符，父进程不需要从ping管道读取
        close(pong[1]);  // 关闭不必要的文件描述符，父进程不需要向pong管道写入

        // 1. 父进程首先向ping管道写入一个字节'!'
        write(ping[1], "!", 1);

        char buf;
        // 2. 父进程发送完成后，开始等待从pong管道读取子进程的回复
        read(pong[0], &buf, 1);

        // 5. 父进程收到数据后，read返回，输出 "received pong"
        printf("%d: received pong\n", getpid());

        close(ping[1]); // 关闭用于写入的ping管道文件描述符
        close(pong[0]); // 关闭用于读取的pong管道文件描述符
    } else { // 子进程
        close(ping[1]); // 关闭不必要的文件描述符，子进程不需要向ping管道写入
        close(pong[0]); // 关闭不必要的文件描述符，子进程不需要从pong管道读取

        char buf;
        // 3. 子进程读取ping管道，收到父进程发送的字节数据
        read(ping[0], &buf, 1);

        // 4. 子进程收到数据后，输出 "received ping"
        printf("%d: received ping\n", getpid());

        // 4. 子进程通过pong管道，将收到的字节数据送回父进程
        write(pong[1], &buf, 1);

        close(ping[0]); // 关闭用于读取的ping管道文件描述符
        close(pong[1]); // 关闭用于写入的pong管道文件描述符
    }
    exit(0);
}
