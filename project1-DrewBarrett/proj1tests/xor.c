#include <stdio.h>
// userspace implementation of xor cipher


void xor(unsigned long key, unsigned char *msg, long n) {
    printf("Key: 0x%lx\n", key);
    // calculate remainder
    int r = n % 4;
    unsigned char fakemsg[n+r];
    for (int i = 0; i < n+r; ++i) {
        if (i >= n) {
            fakemsg[i] = 0;
        }
        else {
            fakemsg[i] = msg[i];
        }
        printf("0x%lx ", fakemsg[i]);
    }
    printf("\n");
    for (int i = 0; i < n+r; i += 4) {
        // create 32 bit long
        unsigned long l = (fakemsg[i] << 24) |
            (fakemsg[i+1] << 16) |
            (fakemsg[i+2] << 8) |
            (fakemsg[i+3]);
        printf("0x%lx\n", l);
        l = l ^ key;
        printf("0x%lx\n", l);
        fakemsg[i] = (0xFF000000 & l) >> 24;
        fakemsg[i+1] = (0x00FF0000 & l) >> 16;
        fakemsg[i+2] = (0x0000FF00 & l) >> 8;
        fakemsg[i+3] = 0x000000FF & l;
    }
    for (int i = 0; i < n; ++i) {
        printf("0x%lx ", fakemsg[i]);
    }
}


int main(int argc, char *argv[])
{
    unsigned char msg[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x12, 0x34};
    xor(0x1BADC0DE, msg, 6);
    return 0;
}
