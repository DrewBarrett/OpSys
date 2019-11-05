#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/kernel.h>
#include <sys/syscall.h>
#include <errno.h>

#define create_mbox_421 335
#define remove_mbox_421 336
#define	count_mbox_421 337
#define list_mbox_421 338
#define send_msg_421 339
#define recv_msg_421 340
#define peek_msg_421 341
#define count_msg_421 342
#define len_msg_421 343

int main(int argc, char *argv[])
{
    long create = syscall(create_mbox_421, 1, 0);
    printf("The following should fail when not run as root: \n"
            "Create: %ld, %d\n", create, errno);
    create = syscall(create_mbox_421, 1, 0);
    printf("Create existing mailbox (should fail) %ld, %d\n",
            create, errno);
    const char *mymessage = "f the police";
    long msg = syscall(send_msg_421, 1, mymessage, 12, 0);
    printf("Send message: %ld, %d, %ld, %s\n", msg, errno, 12, mymessage);
    // send invalid pointer
    msg = syscall(send_msg_421, 1, NULL, 12, 0);
    printf("Send invalid message: %ld, %d\n", msg, errno);
    // send to invalid mailbox
    msg = syscall(send_msg_421, 2, mymessage, 12, 0);
    printf("Send message to wrong box: %ld, %d\n", msg, errno);
    msg = syscall(send_msg_421, 1, mymessage, -1l, 0);
    printf("Send message of wrong size: %ld, %d\n", msg, errno);
    // check mailbox length
    long count = syscall(count_msg_421, 1);
    printf("Check count of mailbox 1 = %ld, %d\n", count, errno);
    // check mailbox length on non existent mailbox
    count = syscall(count_msg_421, 2);
    printf("Check count of invalid mailbox = %ld, %d\n", count, errno);
    long remov = syscall(remove_mbox_421, 1);
    printf("Remove not empty mailbox %ld, %d\n", remov, errno);
    long * mbxes = (long *)malloc(1);
    count = syscall(list_mbox_421, mbxes, 1);
    printf("Get list of mailboxes %ld, %ld,\n", mbxes[0], count);

    // check len_msg
    count = syscall(len_msg_421, 1);
    printf("Check len_msg %ld = %ld, %d\n", 12, count, errno);
    // check peekmsg with incorrect size
    char * recv = (char *)malloc(0);
    msg = syscall(peek_msg_421, 1, recv, count+1, 0);
    printf("Check peek with wrong size %s, %ld, %d\n", recv, msg, errno);
    recv = (char *)malloc(count-1);
    msg = syscall(peek_msg_421, 1, recv, count-1, 0);
    printf("Check small peek %s, %ld, %d\n", recv, msg, errno);
    recv = (char *)malloc(count);
    // check recvmsg
    msg = syscall(recv_msg_421, 1, recv, count, 0);
    printf("Check recv %s, %ld, %d\n", recv, msg, errno);
    // check len_msg of no message
    count = syscall(len_msg_421, 1);
    printf("Check len_msg of no message %ld = %ld, %d\n", 12, count, errno);
    // check len_msg of no mailbox
    count = syscall(len_msg_421, 2);
    printf("Check len_msg of no mb %ld = %ld, %d\n", 12, count, errno);
    // check peekmsg of wrong mbox
    msg = syscall(recv_msg_421, 2, recv, count, 0);
    printf("Check recv of no mb %ld, %d\n", msg, errno);
    // check peekmsg of no msg
    msg = syscall(recv_msg_421, 1, recv, count, 0);
    printf("Check recv of no message %ld, %d\n", msg, errno);
    
    
    count = syscall(count_mbox_421);
    printf("MBOX Count: %ld, %d\n", count, errno);
    remov = syscall(remove_mbox_421, 1);
    printf("Remove %ld, %d\n", remov, errno);
    remov = syscall(remove_mbox_421, 1);
    printf("Remove non existant mailbox (should fail) %ld, %d\n",
            remov, errno);
    count = syscall(count_mbox_421);
    printf("MBOX Count: %ld, %d\n", count, errno);
    // TODO: test sending messages as different processes
    
    count = syscall(create_mbox_421, 1, 1);
    printf("Create cypher mbox: %ld, %d\n", count, errno);
    count = syscall(send_msg_421, 1, mymessage, 12, 0);
    printf("Send noncyphered message: %ld, %d\n", count, errno);
    recv = (char *)malloc(12);
    count = syscall(recv_msg_421, 1, recv, 12, 0xDEADBEEF);
    printf("Recv with wrong cypher: %s, %ld, %d\n", recv, count, errno);
    count = syscall(send_msg_421, 1, "DEADBEEF12345", 13, 0x1BADC0DE);
    printf("Send cyphered message: %ld, %d\n", count, errno);
    recv = (char *)malloc(13);
    count = syscall(recv_msg_421, 1, recv, 13, 0x1BADC0DE);
    printf("Decode cyphered message: %s, %ld, %d\n", recv, count, errno);
    syscall(remove_mbox_421, 1);
    return 0;
}
