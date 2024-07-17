#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAX_LEN 100  // 每个参数的最大长度

int main(int argc, char **argv)
{
    char *command = argv[1];  // 要执行的命令
    char bf;  // 用于读取标准输入的字符缓冲区
    char paramv[MAXARG][MAX_LEN]; // 存储参数的二维数组
    char *m[MAXARG];  // 存储参数指针的数组

    while (1)  // 每次执行一条指令，每行存储的是一个参数
    {
        int count = argc - 1;  // 初始化参数计数器
        memset(paramv, 0, MAXARG * MAX_LEN); // 清空参数数组

        // 将命令行参数复制到paramv数组中
        for (int i = 1; i < argc; i++) {
            strcpy(paramv[i - 1], argv[i]);
        }

        int cursor = 0;  // 当前参数的字符位置
        int flag = 0;  // 用于标记是否在读取参数
        int read_result;

        // 读取标准输入，直到遇到换行符
        while ((read_result = read(0, &bf, 1)) > 0 && bf != '\n') {
            if (bf == ' ' && flag == 1) {  // 遇到空格且flag为1，表示一个参数结束
                count++;  // 增加参数计数
                cursor = 0;  // 重置字符位置
                flag = 0;  // 重置标志
            }
            else if (bf != ' ') {  // 遇到非空格字符
                paramv[count][cursor++] = bf;  // 将字符添加到当前参数
                flag = 1;  // 设置标志
            }
        }

        // 如果读取结果小于等于0，表示到达文件结束符
        if (read_result <= 0) {
            break;
        }

        // 如果遇到换行符，则准备执行命令
        for (int i = 0; i < MAXARG - 1; i++) {
            m[i] = paramv[i];  // 将参数指针复制到m数组中
        }
        m[MAXARG - 1] = 0;  // 以NULL结尾的参数列表

        // 创建子进程执行命令
        if (fork() == 0) {
            exec(command, m);  // 在子进程中执行命令
            exit(0);  // 如果exec失败，退出子进程
        }
        else {
            wait((int *)0);  // 父进程等待子进程完成
        }
    }

    exit(0);  // 退出程序
    return 0;
}
