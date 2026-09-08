#include <stddef.h>
#include <stdio.h>

struct llist_node {
    struct llist_node *next;
};

struct rcu_head {
    struct rcu_head *next;
    void (*func)(struct rcu_head *head);
};

struct vfsmount {};
struct dentry {};

struct path {
    struct vfsmount *mnt;
    struct dentry *dentry;
};

typedef struct {
    int counter;
} atomic_long_t;

typedef int spinlock_t;
typedef int fmode_t;
typedef long long loff_t;
typedef unsigned int u32;
typedef unsigned long long u64;

struct mutex {
    void *owner;
    void *wait_lock;
    void *wait_list[2];
};

struct fown_struct {
    void *lock;
    void *pid;
    int pid_type;
    int uid, euid;
    int signum;
};

struct file_ra_state {
    u64 start;
    unsigned int size;
    unsigned int async_size;
    unsigned int ra_pages;
    unsigned int mmap_miss;
    loff_t prev_pos;
};

struct file {
    union {
        struct llist_node fu_llist;
        struct rcu_head fu_rcuhead;
    } f_u;
    struct path f_path;
    void *f_inode;
    void *f_op;
    spinlock_t f_lock;
    int f_write_hint;
    atomic_long_t f_count;
    unsigned int f_flags;
    fmode_t f_mode;
    struct mutex f_pos_lock;
    loff_t f_pos;
    struct fown_struct f_owner;
    void *f_cred;
    struct file_ra_state f_ra;
    u64 f_version;
    void *f_security;
    void *private_data;
    void *f_ep;
};

int main() {
    printf("f_inode: %lu\\n", offsetof(struct file, f_inode));
    printf("f_op: %lu\\n", offsetof(struct file, f_op));
    printf("f_lock: %lu\\n", offsetof(struct file, f_lock));
    printf("f_count: %lu\\n", offsetof(struct file, f_count));
    printf("f_version: %lu\\n", offsetof(struct file, f_version));
    printf("private_data: %lu\\n", offsetof(struct file, private_data));
    printf("f_ep: %lu\\n", offsetof(struct file, f_ep));
    return 0;
}
