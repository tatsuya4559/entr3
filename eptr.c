#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <err.h>
#include <sys/inotify.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

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

static void register_path_to_watch(int inotify_fd, const char *path) {
  printf("add path %s\n", path);
  int watch_descriptor = inotify_add_watch(
      inotify_fd,
      path,
      IN_MODIFY | IN_CREATE | IN_DELETE);
  if (watch_descriptor == -1) {
    err(EXIT_FAILURE, "inotify_add_watch");
  }
}

static bool is_file_or_dir(const char *path);

int main(int argc, char **argv) {
  if (argc < 2) {
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }
  // expect file list from a pipe
  if (isatty(STDIN_FILENO)) {
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  setup_signal_handler();

  int inotify_fd = inotify_init();
  if (inotify_fd == -1) {
    err(EXIT_FAILURE, "inotify_init");
  }

  char path[PATH_MAX];
  char *p;
  struct stat file_info;
  while (fgets(path, PATH_MAX, stdin)) {
    if (path[0] == '\0') {
      continue;
    }

    // remove newline
    if ((p = strchr(path, '\n')) != NULL) {
      *p = '\0';
    }

    if (stat(path, &file_info) == -1) {
      fprintf(stderr, "Cannot check stat on %s\n", path);
      continue;
    }
    if (S_ISREG(file_info.st_mode) || S_ISDIR(file_info.st_mode)) {
      register_path_to_watch(inotify_fd, path);
    } else {
      // ignore
      continue;
    }
  }

  char buffer[BUF_LEN];
  for (;;) {
    if (read(inotify_fd, buffer, BUF_LEN) == -1) {
      err(EXIT_FAILURE, "read");
    }

    printf("changed\n");
    /* run_utility(); */
  }

  return EXIT_SUCCESS;
}
