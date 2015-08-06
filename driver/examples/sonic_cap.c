#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "sonic.h"

#define SONICPORT0	"/dev/sonic_port0"
#define SONICPORT1	"/dev/sonic_port1"

static void exit_fail_msg(char * msg) 
{
    fprintf(stderr, "%s:%s\n", msg, strerror(errno));
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    int fd, ret, i, j, port;
    unsigned long long l=0;
    char *dev;
    struct sonic_app_list *app_list;
    struct sonic_cap_entry *cap_entry;
    FILE *out;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> <output>\n", argv[0]);
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

    out= fopen(argv[2], "w");

    app_list = malloc(4096 * 16);
    if (!app_list)
        exit_fail_msg("Memory Error");

    if ((fd = open(dev, O_RDONLY)) == -1)
        exit_fail_msg("File Opening Error");

    while((ret = read(fd, app_list, 4096 * 16))) {
        cap_entry = (struct sonic_cap_entry *) app_list->entry;

        for ( i = 0 ; i < app_list->cnt ; i ++ ) {
            fprintf(out, "%u %u %u %llu ", 
                    cap_entry->type, cap_entry->len, cap_entry->idle, 
                    (unsigned long long )cap_entry->idle_bits);

            for ( j = 0 ; j < 48 ; j ++ ) {
                fprintf(out, "%.2x", cap_entry->buf[j]);

                if (j % 4 == 3)
                    fprintf(out, " ");
            }
            fprintf(out, "\n");

            cap_entry++;

        }
        l += i;
    }

    close(fd);

    fclose(out);

    exit(EXIT_SUCCESS);
}
