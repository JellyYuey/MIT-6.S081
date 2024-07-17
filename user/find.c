#include "kernel/types.h"      // 包含类型定义
#include "kernel/stat.h"       // 包含文件状态信息定义
#include "user/user.h"         // 包含用户级别的系统调用接口
#include "kernel/fs.h"         // 包含文件系统结构定义
#include "kernel/fcntl.h"      // 包含文件控制选项定义

// 定义 find 函数，递归查找指定目录下的文件
void find(char *path, char *target_file);

int main(int argc, char **argv)
{
    // 检查参数数量是否正确
    if (argc != 3)
    {
        // 错误处理：传入的参数数量不正确时，输出错误信息并退出
        fprintf(2, "error: You need to pass in exactly 2 arguments\n");
        exit(1);
    }

    char *target_path = argv[1];  // 获取第一个参数（目标路径）
    char *target_file = argv[2];  // 获取第二个参数（目标文件）
    find(target_path, target_file);  // 调用 find 函数进行查找
    exit(0);  // 正常退出程序
    return 0;  // 返回值（不必要，因为前面已经 exit 了）
}

void find(char *path, char *target_file)
{
    char buf[512], *p;  // 定义缓冲区和指针变量
    int fd;  // 文件描述符
    struct dirent de;  // 目录项结构
    struct stat st;  // 文件状态结构

    // 打开指定路径，获取文件描述符
    if ((fd = open(path, 0)) < 0)
    {
        // 错误处理：无法打开路径时，输出错误信息
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    // 获取文件状态信息
    if (fstat(fd, &st) < 0)
    {
        // 错误处理：无法获取文件状态时，输出错误信息并关闭文件描述符
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    // 根据文件类型进行处理
    switch (st.type)
    {
    case T_FILE:  // 如果是文件类型
        fprintf(2, "Usage: find dir file\n");  // 输出错误信息：应输入文件夹
        exit(1);  // 退出程序
        break;

    case T_DIR:  // 如果是目录类型
        // 检查路径长度是否超出缓冲区大小
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
        {
            printf("find: path too long\n");  // 输出错误信息：路径过长
            break;
        }

        strcpy(buf, path);  // 复制路径到缓冲区
        p = buf + strlen(buf);  // 移动指针到缓冲区末尾
        *p++ = '/';  // 添加目录分隔符

        // 读取目录项
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            // 跳过无效的目录项和特殊目录项 . 和 ..
            if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;

            // 将文件名加入缓冲区路径
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;  // 添加字符串结束符

            // 获取文件状态信息
            if (stat(buf, &st) < 0)
            {
                printf("find: cannot stat %s\n", buf);  // 错误处理：无法获取文件状态
                continue;
            }

            if (st.type == T_DIR)  // 如果是目录，递归查找
            {
                find(buf, target_file);
            }
            else if (st.type == T_FILE)  // 如果是文件，检查是否是目标文件
            {
                if (strcmp(de.name, target_file) == 0)
                {
                    printf("%s\n", buf);  // 如果匹配，输出文件路径
                }
            }
        }
        break;
    }

    close(fd);  // 关闭文件描述符
}
