
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include "oslab.h"

void print_help(const char *program)
{
    printf("Dining Philosophers: Deadlock Prevention System\n\n");
    printf("Usage: %s [simulate] [options]\n\n", program);

    printf("Options:\n");
    printf("  --philosophers N    Number of philosophers (2-20)\n");
    printf("  --meals N           Meals per philosopher (0 = until interrupted)\n");
    printf("  --strategy waiter   FIFO waiter synchronization (default)\n");
    printf("  --strategy ordered  Resource ordering synchronization\n");
    printf("  --log PATH          Save simulation events to a file\n");
    printf("  --quiet             Display final results only\n");
    printf("  --help              Display this help\n\n");

    printf("Examples:\n");
    printf("  %s simulate --philosophers 5 --meals 3\n", program);
    printf("  %s simulate --strategy ordered --meals 2 --quiet\n", program);
    printf("  %s simulate --meals 0\n", program);
}

int main(int argc, char **argv)
{
    if (argc > 1 &&
        (strcmp(argv[1], "--help") == 0 ||
         strcmp(argv[1], "-h") == 0 ||
         strcmp(argv[1], "help") == 0)) {
        print_help(argv[0]);
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "simulate") != 0) {
        fprintf(stderr, "Unknown command: %s\n\n", argv[1]);
        print_help(argv[0]);
        return 2;
    }

    run_philosophers(argc, argv);
    return 0;
}

