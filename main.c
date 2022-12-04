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
    1.解析args
    2.遍历/proc/{pid}/stat，读出每一个process_stat，根据pid和ppid关系挂树
    3.打印
    4.释放资源
 */

/* 
    todo
    解析args
    插入排序
    打印美化，涉及到树的叶子节点数，深度
 */

typedef struct
{
    pid_t pid;
    char comm[64];
    char state;
    pid_t ppid;
}process_stat;

typedef struct _Node{
    struct _Node *children;
    struct _Node *next_brother;
    process_stat stat;
}Node;

static Node head;

typedef int (*node_iter_func)(Node *node, void *user);

void iter_node_child(Node *head, node_iter_func func, void *user)
{
    Node *cur = head, *child = NULL, *bro = NULL;

    while (cur)
    {
        child = cur->children;
        bro = cur->next_brother;
        if(func(cur, user) != 0) {
            break;
        }
        if(child)
            iter_node_child(child, func, user);
        cur = bro;
    }
}

void iter_node_bro(Node *head, node_iter_func func, void *user)
{
    Node *cur = head, *child = NULL, *bro = NULL;

    while (cur)
    {
        child = cur->children;
        bro = cur->next_brother;
        if(func(cur, user) != 0) {
            break;
        }
        if(bro)
            iter_node_bro(bro, func, user);
        cur = child;
    }
}

Node *create_node_with_stat(process_stat *stat)
{
    Node *tmp = (Node *)malloc(sizeof(Node));
    if (!tmp) {
        return NULL;
    }
    tmp->next_brother = NULL;
    tmp->children = NULL;
    memcpy(&tmp->stat, stat, sizeof(process_stat));
    return tmp;
}

/* 
    头插法
 */

int insert_child_head(Node *node, void *user)
{
    process_stat *p_stat = (process_stat *)user;
    if(node->stat.pid == p_stat->ppid) {
        if(!node->children) {
            node->children = create_node_with_stat(p_stat);
        } else {
            Node *tmp = node->children->next_brother;
            node->children->next_brother = create_node_with_stat(p_stat);
            node->children->next_brother->next_brother = tmp;
        }
        return 1;
    }
    return 0;
}

/* 
    有序插入
 */

int insert_child_ordered(Node *node, void *user)
{
    process_stat *p_stat = (process_stat *)user;
    if(node->stat.pid == p_stat->ppid) {
        if(!node->children) {
            node->children = create_node_with_stat(p_stat);
        } else {
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
    if (!f) {
        /*
            可能这时候程序关闭了
         */
        fprintf(stderr, "open %s failed due to %s", stat_file_buf, strerror(errno));
        return;
    }

    process_stat stat;
    int ret = fscanf(f, "%d %s %c %d", &stat.pid, stat.comm, &stat.state, &stat.ppid);
    assert(ret == 4);
    iter_node_bro(&head, insert_child_head, &stat);
    fclose(f);
}

int main(int argc, char **argv) 
{
    int ret = EINVAL;
    DIR *proc_dir = opendir("/proc");
    if (!proc_dir) {
        perror("opendir");
        return -1;
    }


    struct dirent *ptr = NULL;

    while((ptr = readdir(proc_dir)) != NULL) {
        if (ptr->d_type == DT_DIR && isdigit(ptr->d_name[0])) {
            handle_proc_stat(ptr->d_name);
        }
    }

    iter_node_bro(&head, print_node, NULL);

    iter_node_bro(head.children, free_node, NULL);
    
    ret = 0;
exit:
    closedir(proc_dir);

    return ret;
}
