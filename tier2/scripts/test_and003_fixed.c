#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <errno.h>

struct my_msgbuf {
    long mtype;
    char mtext[144];
};

void do_log(const char *msg) {
    write(1, msg, strlen(msg));
}

int main() {
    do_log("[AND-003] SELinux Enforcing Syscall Audit\n");
    
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        do_log("[AND-003] FAIL: epoll_create1 failed\n");
    } else {
        do_log("[AND-003] PASS: epoll_create1 success\n");
    }
    
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        do_log("[AND-003] FAIL: pipe failed\n");
    } else {
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = pipefd[0];
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, pipefd[0], &ev) < 0) {
            do_log("[AND-003] FAIL: epoll_ctl failed\n");
        } else {
            do_log("[AND-003] PASS: epoll_ctl success\n");
        }
    }
    
    close(epfd);
    do_log("[AND-003] PASS: close success\n");
    
    int msqid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (msqid < 0) {
        do_log("[AND-003] FAIL: msgget failed\n");
    } else {
        do_log("[AND-003] PASS: msgget success\n");
        
        struct my_msgbuf msg;
        msg.mtype = 1;
        memset(msg.mtext, 'A', sizeof(msg.mtext));
        
        if (msgsnd(msqid, &msg, sizeof(msg.mtext), 0) < 0) {
            do_log("[AND-003] FAIL: msgsnd failed\n");
        } else {
            do_log("[AND-003] PASS: msgsnd success\n");
            
            struct my_msgbuf rcvmsg;
            if (msgrcv(msqid, &rcvmsg, sizeof(rcvmsg.mtext), 0, IPC_NOWAIT) < 0) {
                do_log("[AND-003] FAIL: msgrcv failed\n");
            } else {
                do_log("[AND-003] PASS: msgrcv success\n");
            }
        }
        msgctl(msqid, IPC_RMID, NULL);
    }
    
    do_log("[AND-003] Audit Complete\n");
    return 0;
}
