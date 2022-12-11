#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <linux/sched.h>
#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

#define LINE_MAX 16
#define NODE_MAX 64
#define LINE_STR_SIZE 1024

typedef struct
{
    pid_t pid;
    char comm[64];
    char state;
    pid_t ppid;
    int height;
} process_stat;

typedef struct _Node
{
    struct _Node *children;
    struct _Node *brother;
    process_stat stat;
} Node;

/*
    按行format，有bro节点的地方，用+衔接，其他地方用-衔接

    第一步，我们按行输出所有的路径
    第二步，把和上面一行“重复”的路径去掉
    第三步，把分叉的节点符号替换掉
 */

typedef struct _Node_path_buff
{
    Node *path_lines[NODE_MAX];
    int node_num;
} Node_path_buff;

static Node head = {
    .stat.height = -1,
    .stat.pid = 0
};

static Node *node_path_buffer[NODE_MAX];

/*
    返回0终止迭代
 */
typedef int (*node_iter_func)(Node *node, void *user);

void print_nodes(Node **buffer, int len)
{
    char buf[BUFSIZ];
    int offset = 0, rc = 0;
    const char *connecter = "---";
    for (int i = 0; i < len; i++)
    {
        offset += sprintf(buf + offset, "%s(%d)%s", buffer[i]->stat.comm,
                          buffer[i]->stat.pid, connecter);
    }
    buf[offset] = '\0';
    printf("%s\n", buf);
}

void preOrder_print(Node *head, Node **buffer, int len)
{
    if (!head)
        return;

    buffer[len] = head;
    len++;

    if (!head->children)
    {
        print_nodes(buffer, len);
    }

    preOrder_print(head->children, buffer, len);
    preOrder_print(head->brother, buffer, len);
}

/*
example:

init(1)-+-init(7)---init(8)---bash(9)
        |-init(232)---init(233)-+-sh(234)---sh(235)---sh(240)---node(244)-+-node(264)-+-bash(493)
        |                       |                                         |
`-bash(781)---pstree(2577) |                       | |-node(302)-+-cpptools(466)
        |                       |                                         |
`-node(574) |                       | `-node(313) | `-cpptools-srv(2539)
        |-init(255)---init(256)---node(257)
        `-init(292)---init(293)---node(295)

观察得出，我们需要找到每一行即有child，又有child.next_brother时的位置。也就是图中的'+'的位置。因为在下一行里，上一行的'+'会变为'|'，而本行开始的位置，也就正是上一行的'+'
 */


void print_node(Node *node, int offset, int is_bro)
{
    if (!node)
        return;

    int prev_offset = offset;
    if (is_bro)
    {
        putchar('\n');
        for (int i = 0; i < offset - 2; i++)
        {
            putchar(' ');
        }
        if (node->brother)
        {
            putchar('|');
        }
        else
        {
            putchar('`');
        }
        putchar('-');
    }

    offset += printf("%s(%d)", node->stat.comm, node->stat.pid);

    if (!node->children)
    {

    }
    else if (node->children->brother)
    {
        offset += printf("-+-");
    }
    else
    {
        offset += printf("---");
    }

    print_node(node->children, offset, 0);
    print_node(node->brother, prev_offset, 1);
}

void preOrder_print2(Node *node, int offset, int brother_offset)
{
    if (!node)
        return;

    offset += printf("%s(%d)", node->stat.comm, node->stat.pid);
    if (node->children && node->children->brother)
    {
        offset += printf("-+-");
        brother_offset = offset - 1;
    }
    else if (node->children)
    {
        offset += printf("---");
    }
    else if (!node->children)
    {
        printf("\n");
        for (int i = 0; i < brother_offset; i++)
        {
            putchar(' ');
        }
    }

    preOrder_print2(node->children, offset, brother_offset);
    preOrder_print2(node->brother, brother_offset, brother_offset);
}

void preOrder(Node *head, node_iter_func func, void *user)
{
    if (!head)
        return;

    Node *child = head->children, *bro = head->brother;

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
    tmp->brother = NULL;
    tmp->children = NULL;
    memcpy(&tmp->stat, stat, sizeof(process_stat));
    return tmp;
}

/*
    头插法
    如果用STL的思路来做的话，迭代是不能改变容器大小的。。。
 */

#define INSERT_TO_HEAD 0x01
#define INSERT_TO_TAIL 0x02

static int INSERT_MODE = INSERT_TO_TAIL;

void insert_new_node_head(Node *node, Node *new)
{
    if (!node->children)
    {
        node->children = new;
    }
    else
    {
        Node *tmp = node->children->brother;
        node->children->brother = new;
        node->children->brother->brother = tmp;
    }
}

void insert_new_node_tail(Node *node, Node *new)
{
    if (!node->children)
    {
        new->stat.height = node->stat.height + 1;
        node->children = new;
    }
    else if (!node->children->brother)
    {
        new->stat.height = node->children->stat.height;
        node->children->brother = new;
    }
    else
    {
        Node *tmp = node->children->brother;
        while (tmp->brother)
        {
            tmp = tmp->brother;
        }
        new->stat.height = node->children->stat.height;
        tmp->brother = new;
    }
}

int insert_new_node(Node *node, void *user)
{
    process_stat *p_stat = (process_stat *)user;
    if (node->stat.pid != p_stat->ppid)
    {
        return 0;
    }

    Node *new = create_node_with_stat(p_stat);
    if (!new)
    {
        // todo no memory
        return 1;
    }

    INSERT_MODE == INSERT_TO_TAIL ? insert_new_node_tail(node, new) : insert_new_node_head(node, new);

    return 1;
}

int print_node_simple(Node *n, void *null)
{
    printf("cur pid %d ppid %d height %d cmd %s child %p\n", n->stat.pid, n->stat.ppid, n->stat.height, n->stat.comm, n->children);
    return 0;
}

int print_node_liketree(Node *n, void *buffer)
{
    Node_path_buff *path_buffer = (Node_path_buff *)buffer;
    path_buffer->path_lines[path_buffer->node_num] = n;
    path_buffer->node_num;

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
    int ret =
        fscanf(f, "%d %s %c %d", &stat.pid, stat.comm, &stat.state, &stat.ppid);
    int len = strlen(stat.comm);
    memmove(stat.comm, stat.comm + 1, len - 2);
    stat.comm[len - 2] = '\0';
    assert(ret == 4);
    preOrder(&head, insert_new_node, &stat);
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

    print_node(head.children, 0, 0);
    putchar('\n');

    preOrder(head.children, free_node, NULL);

    ret = 0;
exit:
    closedir(proc_dir);

    return ret;
}
