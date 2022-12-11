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

#define LINE_MAX 16
#define NODE_MAX 64
#define LINE_STR_SIZE 1024

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
    struct _Node *brother;
    process_stat stat;
} Node;

/* 
    按行format，有bro节点的地方，用+衔接，其他地方用-衔接
    
    第一步，我们按行输出所有的路径
    第二步，把和上面一行“重复”的路径去掉
    第三步，把分叉的节点符号替换掉
 */

typedef struct _Node_path_buff {
    Node *path_lines[NODE_MAX];
    int node_num;
} Node_path_buff;

static Node head;

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
    for(int i = 0; i < len; i++) {
        offset += sprintf(buf + offset, "%s(%d)%s", buffer[i]->stat.comm, buffer[i]->stat.pid, connecter);
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

    if(!head->children)
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
        |                       |                                         |           `-bash(781)---pstree(2577)
        |                       |                                         |-node(302)-+-cpptools(466)
        |                       |                                         |           `-node(574)
        |                       |                                         `-node(313)
        |                       `-cpptools-srv(2539)
        |-init(255)---init(256)---node(257)
        `-init(292)---init(293)---node(295)

观察得出，我们需要找到每一行即有child，又有child.next_brother时的位置。也就是图中的'+'的位置。因为在下一行里，上一行的'+'会变为'|'，而本行开始的位置，也就正是上一行的'+'
 */

void print_child_bro(Node *node, int offset, int brother_offset, int is_bro)
{
    if (!node)
        return;

    if(is_bro){
        putchar('\n');
        for(int i = 0; i < offset - 1; i++){
            putchar(' ');
        }
        if (node->brother){
            putchar('|');
        } else {
            putchar('`');
        }
        putchar('-');
        brother_offset = offset;
    }

    offset += printf("%s(%d)", node->stat.comm, node->stat.pid);
    if (node->children && node->children->brother) 
    {
        offset += printf("-+-");
        brother_offset = offset;
    }
    else if (node->children)
    {
        offset += printf("---");
    }

    print_child_bro(node->children, offset, brother_offset, 0);
    print_child_bro(node->brother, brother_offset, 0, 1);
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
    } else if (!node->children)
    {
        printf("\n");
        for (int i = 0; i < brother_offset; i++) {
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
            Node *tmp = node->children->brother;
            node->children->brother = create_node_with_stat(p_stat);
            node->children->brother->brother = tmp;
        }
        return 1;
    }
    return 0;
}

int insert_child_tail(Node *node, void *user)
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
            Node *tmp = node->children->brother;
            if (tmp) {
                while (tmp->brother) {
                    tmp = tmp->brother;
                }
                tmp->brother = create_node_with_stat(p_stat);
            } else {
                node->children->brother = create_node_with_stat(p_stat);
            }
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
            Node *tmp = node->children->brother;
            node->children->brother = create_node_with_stat(p_stat);
            node->children->brother->brother = tmp;
        }
        return 1;
    }
    return 0;
}

int print_node_simple(Node *n, void *null)
{
    // printf("cur pid %d ppid %d cmd %s child %p\n", n->stat.pid, n->stat.ppid, n->stat.comm, n->children);
    if(n->children){
        printf("%s(%d)->",n->stat.comm ,n->stat.pid);
    } else {
        printf("%s(%d)\n",n->stat.comm, n->stat.pid);
    }
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
    int ret = fscanf(f, "%d %s %c %d", &stat.pid, stat.comm, &stat.state, &stat.ppid);
    int len = strlen(stat.comm);
    memmove(stat.comm, stat.comm + 1, len - 2);
    stat.comm[len - 2] = '\0';
    assert(ret == 4);
    preOrder(&head, INSERT_MODE == INSERT_TO_TAIL ? insert_child_tail : insert_child_head, &stat);
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

    // preOrder(head.children, print_node_simple, NULL);
    // tree_str_buff tree_buff;
    // bzero(&tree_buff, sizeof tree_buff);
    // preOrder(head.children, print_node_liketree, &tree_buff);

    // preOrder_print(head.children, node_path_buffer, 0);
    // preOrder_print2(head.children, 0, 0);
    print_child_bro(head.children, 0, 0, 0);
    putchar('\n');

    preOrder(head.children, free_node, NULL);

    ret = 0;
exit:
    closedir(proc_dir);

    return ret;
}
