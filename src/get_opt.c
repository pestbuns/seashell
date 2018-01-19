#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "seashell.h"


static void print_usage(const char *binary_name)
{
    printf("Usage: %s [-h] Print seashell help\n", binary_name);
}


static void print_help(const char *binary_name)
{
    printf("Usage: %s\n", binary_name);
    printf("\n=== S E A S H E L L ===\n");
    printf("\nFeatures List:\n");
    printf("\n=INTERFACE MANAGEMENT=\n");
    printf("cursor motion:DONE!\n");
    printf("- cursor left:done\n");
    printf("- cursor right:done\n");
    printf("buffer management:DONE!\n");
    printf("- insert_char:done\n");
    printf("- remove_char:done\n");
    printf("autocompletion:TODO!\n");
    printf("-tab key management:todo\n");
    printf("jobcontrols:TODO!\n");
    printf("history: TODO!\n");
    printf("- cursor up: done\n");
    printf("- cursor down: done\n");
    printf("- don't fill list if same last entry: todo\n");
    printf("- implement history rotation with a limited size: todo\n");
    printf("- history based on .history file for persistency: todo\n");
    printf("\n=EXECUTION MANAGEMENT=\n");
    printf("bultins:TODO!\n");
    printf("- cd:todo\n");
    printf("- echo:todo\n");
    printf("- exit:todo\n");
    printf("redirection:TODO!\n");
    printf("- >:todo\n");
    printf("- >>:todo\n");
    printf("- <:todo\n");
    printf("multipipes:DONE!\n");
    printf("\n=CONFIGURATION MANAGEMENT=\n");
    printf("- prompt management:todo\n");
    printf("- path to history:todo\n");
    printf("- aliases:todo\n");
}

int get_arguments(int argc, char *argv[])
{
    int c = 0;
    int option_index = 0;

    static struct option long_options[] = {
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    while ((c = getopt_long(argc, argv, "h", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h': print_help(argv[0]); exit(EXIT_SUCCESS);
            default: print_usage(argv[0]); exit(EXIT_SUCCESS);
        }
    }

    return 0;
}