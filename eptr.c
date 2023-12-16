#include <stdlib.h>

/* prototypes */
void show_usage(void);

int main(int argc, char **argv) {
  if (argc < 2) {
    show_usage();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
