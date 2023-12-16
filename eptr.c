#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <err.h>

static void print_usage(char *cmd) {
  fprintf(stderr, "%s utility [arguments...]\n", cmd);
}

static void handle_exit(int sig) {
  // TODO: terminate utility
  if (sig == SIGINT || sig == SIGHUP) {
    _exit(0);
  }
  raise(sig);
}

static void setup_signal_handler(void) {
  struct sigaction act;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_RESETHAND;
  act.sa_handler = handle_exit;
  if (sigaction(SIGINT, &act, NULL) != 0) {
    err(EXIT_FAILURE, "Failed to set SIGINT handler");
  }
  if (sigaction(SIGTERM, &act, NULL) != 0) {
    err(EXIT_FAILURE, "Failed to set SIGTERM handler");
  }
  if (sigaction(SIGHUP, &act, NULL) != 0) {
    err(EXIT_FAILURE, "Failed to set SIGHUP handler");
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }
  /* expect file list from a pipe */
  if (isatty(STDIN_FILENO)) {
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  setup_signal_handler();

  return EXIT_SUCCESS;
}
