#include <stdio.h>
#include <stdlib.h>

static void print_usage(char *cmd) {
  fprintf(stderr, "%s utility [arguments...]\n", cmd);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
