#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/sched.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <ctype.h>

/*
    首先需要找一个合理的数据结构，然后确定如何迭代他。需要实现的两个迭代函数：

    既然都是树了，那肯定是普通树啦，而普通树使用左孩子右兄弟的结构就转换成了二叉树

    1. 遍历/proc/{pid}/stat，读出每一个process_stat, 根据pid和ppid挂树
    2. 打印
    最终结果要像pstree -pT和pstree -pTn靠齐

    最后去看一下GNU的实现，肯定很牛逼
 */

/*
    todo
    解析args
    插入排序
    打印
 */

/*
    man proc -> /proc/{pid}/stat
 */
typedef struct
{
    pid_t pid;
    char comm[64];
    char state;
    pid_t ppid;
} process_stat;

typedef struct _Node
{
    struct _Node *children;
    struct _Node *next_brother;
    process_stat stat;
} Node;

static Node head;

/*
    返回0终止迭代
 */
typedef int (*node_iter_func)(Node *node, void *user);

void preOrder(Node *head, node_iter_func func, void *user)
{
    if (!head)
        return;

    Node *child = head->children, *bro = head->next_brother;
    
    if (func(head, user) != 0) 
    {
        return;
    }

    preOrder(child, func, user);
    preOrder(bro, func, user);
}

Node *create_node_with_stat(process_stat *stat)
{
    Node *tmp = (Node *)malloc(sizeof(Node));
    if (!tmp)
    {
        return NULL;
    }
    tmp->next_brother = NULL;
    tmp->children = NULL;
    memcpy(&tmp->stat, stat, sizeof(process_stat));
    return tmp;
}

/*
    头插法
    如果用STL的思路来做的话，迭代是不能改变容器大小的。。。
 */

int insert_child_head(Node *node, void *user)
{
    process_stat *p_stat = (process_stat *)user;
    if (node->stat.pid == p_stat->ppid)
    {
        if (!node->children)
        {
            node->children = create_node_with_stat(p_stat);
        }
        else
        {
            Node *tmp = node->children->next_brother;
            node->children->next_brother = create_node_with_stat(p_stat);
            node->children->next_brother->next_brother = tmp;
        }
        return 1;
    }
    return 0;
}

/*
    todo
    有序插入
 */

int insert_child_ordered(Node *node, void *user)
{
    process_stat *p_stat = (process_stat *)user;
    if (node->stat.pid == p_stat->ppid)
    {
        if (!node->children)
        {
            node->children = create_node_with_stat(p_stat);
        }
        else
        {
            Node *tmp = node->children->next_brother;
            node->children->next_brother = create_node_with_stat(p_stat);
            node->children->next_brother->next_brother = tmp;
        }
        return 1;
    }
    return 0;
}

int print_node(Node *n, void *null)
{
    printf("cur pid %d ppid %d cmd %s\n", n->stat.pid, n->stat.ppid, n->stat.comm);
    return 0;
}

int free_node(Node *n, void *null)
{
    free(n);
    return 0;
}

void handle_proc_stat(const char *pid_dir)
{
    assert(pid_dir);
    char stat_file_buf[256];
    sprintf(stat_file_buf, "/proc/%s/stat", pid_dir);

    FILE *f = fopen(stat_file_buf, "r");
    if (!f)
    {
        /*
            可能这时候程序关闭了
         */
        fprintf(stderr, "open %s failed due to %s", stat_file_buf, strerror(errno));
        return;
    }

    process_stat stat;
    int ret = fscanf(f, "%d %s %c %d", &stat.pid, stat.comm, &stat.state, &stat.ppid);
    assert(ret == 4);
    preOrder(&head, insert_child_head, &stat);
    fclose(f);
}

int main(int argc, char **argv)
{
    int ret = EINVAL;
    DIR *proc_dir = opendir("/proc");
    if (!proc_dir)
    {
        perror("opendir");
        return -1;
    }

    struct dirent *ptr = NULL;

    while ((ptr = readdir(proc_dir)) != NULL)
    {
        if (ptr->d_type == DT_DIR && isdigit(ptr->d_name[0]))
        {
            handle_proc_stat(ptr->d_name);
        }
    }

    preOrder(&head, print_node, NULL);

    preOrder(head.children, free_node, NULL);

    ret = 0;
exit:
    closedir(proc_dir);

    return ret;
}
