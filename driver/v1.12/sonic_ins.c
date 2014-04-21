#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "sonic.h"

#define SONICPORT0	"/home/kslee/dev/sonic_port0"
#define SONICPORT1	"/home/kslee/dev/sonic_port1"

static void exit_fail_msg(char * msg) 
{
    fprintf(stderr, "%s:%s\n", msg, strerror(errno));
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    int fd, ret, i, port;
//    unsigned long long l=0;
    char *dev;
    struct sonic_ins *ins;
    struct sonic_ins_entry *ins_entry;
    FILE *fin;
    char buf[256];
    int first=0;
    int total=0;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> <input>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    port = atoi(argv[1]);
    if (port == 0)
        dev = SONICPORT0;
    else if (port == 1)
        dev = SONICPORT1;
    else {
        fprintf(stderr, "port number must be zero or one\n");
        exit(EXIT_FAILURE);
    }

//    printf("%d\n", 4096 * 16 / sizeof(struct sonic_ins_entry) -1);
 //   exit (EXIT_FAILURE);

    ins = malloc(4096 * 16);
    if (!ins)
        exit_fail_msg("Memory Error");

    fin= fopen(argv[2], "r");

    i =0;

    if ((fd = open(dev, O_WRONLY)) == -1)
        exit_fail_msg("File Opening Error");

    // read from info
    while(fgets(buf, 256, fin)) {
        int pid, ptype, plen, pidle;
        if (buf[0] == '#')
            continue;

        sscanf(buf, "%d %d %d %d", &pid, &ptype, &plen, &pidle);

        if (!first) 
            if (ptype == 1) 
                pidle = 12; // FIXME
        first=1;
        
//        printf("%d %d %d %d\n", pid, ptype, plen, pidle); 

        ins_entry = &ins->inss[i];
//        printf("%p\n", ins_entry);

        ins_entry->type = ptype;
        ins_entry->len = plen;
        ins_entry->idle = pidle;
        ins_entry->id = pid;

        i++;

        if (i == 4096 * 16 / sizeof(struct sonic_ins_entry) -2) {
            // Write to character device
            ins->ins_cnt = i;

            ret = write(fd, ins, 4096*16);
            if (ret < 0) 
                exit_fail_msg("Write Error");

            total += i;
            i = 0;
        }
    }

    if (i) {
        ins->ins_cnt = i;
        ret = write(fd, ins, 4096 * 16);
        if (ret < 0)
            exit_fail_msg("Write Error");

        total += i;
    }

    printf("%d passed\n", total);

    close(fd);

    fclose(fin);

    exit(EXIT_SUCCESS);
}
