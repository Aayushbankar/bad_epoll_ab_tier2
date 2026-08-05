#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/reboot.h>

struct msg_test_buf {
    long mtype;
    char mtext[128];
};

int main(void) {
    printf("[AND-001] Starting SysV IPC Availability Test...\n");

    int msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
    if (msqid < 0) {
        printf("[AND-001] FAIL: msgget failed: errno=%d (%s)\n", errno, strerror(errno));
        reboot(RB_POWER_OFF);
        return 1;
    }
    printf("[AND-001] SUCCESS: msgget allocated msqid=%d\n", msqid);

    struct msg_test_buf snd_msg;
    snd_msg.mtype = 1;
    memset(snd_msg.mtext, 'A', 120);
    snd_msg.mtext[120] = '\0';

    if (msgsnd(msqid, &snd_msg, 120, 0) < 0) {
        printf("[AND-001] FAIL: msgsnd failed: errno=%d (%s)\n", errno, strerror(errno));
        msgctl(msqid, IPC_RMID, NULL);
        reboot(RB_POWER_OFF);
        return 1;
    }
    printf("[AND-001] SUCCESS: msgsnd sent 120-byte message to msqid=%d\n", msqid);

    struct msg_test_buf rcv_msg;
    memset(&rcv_msg, 0, sizeof(rcv_msg));
    ssize_t ret = msgrcv(msqid, &rcv_msg, sizeof(rcv_msg.mtext), 1, 0);
    if (ret < 0) {
        printf("[AND-001] FAIL: msgrcv failed: errno=%d (%s)\n", errno, strerror(errno));
        msgctl(msqid, IPC_RMID, NULL);
        reboot(RB_POWER_OFF);
        return 1;
    }
    printf("[AND-001] SUCCESS: msgrcv received %zd bytes, mtext[0]='%c'\n", ret, rcv_msg.mtext[0]);

    if (msgctl(msqid, IPC_RMID, NULL) < 0) {
        printf("[AND-001] FAIL: msgctl IPC_RMID failed: errno=%d (%s)\n", errno, strerror(errno));
        reboot(RB_POWER_OFF);
        return 1;
    }
    printf("[AND-001] SUCCESS: msgctl IPC_RMID freed msqid=%d\n", msqid);

    printf("[AND-001] ALL SYSV IPC SYSCALLS VERIFIED FUNCTIONAL ON TARGET KERNEL\n");
    reboot(RB_POWER_OFF);
    return 0;
}
