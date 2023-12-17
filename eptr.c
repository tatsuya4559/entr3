#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <err.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

static void print_usage(char *cmd) {
  fprintf(stderr, "%s utility [arguments...]\n", cmd);
}

static pid_t child_pid = 0;

static void terminate_utility() {
  if (child_pid == 0) {
    return;
  }

  killpg(child_pid, SIGTERM);
  int status;
  waitpid(child_pid, &status, 0);
  child_pid = 0;
}

static void run_utility(char **argv) {
  terminate_utility();

  pid_t pid = fork();
  if (pid == -1) {
    err(EXIT_FAILURE, "cannot fork");
  }

  if (pid == 0) {
    // child process
    fflush(stdout);
    if (execvp(argv[0], argv) == -1) {
      err(EXIT_FAILURE, "execvp");
    }
  }

  // parent process
  child_pid = pid;
}

static void handle_exit(int sig) {
  terminate_utility();
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

static bool is_file_or_dir(const char *path) {
  struct stat file_info;
  if (stat(path, &file_info) == -1) {
    return false;
  }
  return S_ISREG(file_info.st_mode) || S_ISDIR(file_info.st_mode);
}

static void register_path_to_watch(int inotify_fd, const char *path) {
  int watch_descriptor = inotify_add_watch(
      inotify_fd,
      path,
      IN_MODIFY | IN_CREATE | IN_DELETE);
  if (watch_descriptor == -1) {
    err(EXIT_FAILURE, "inotify_add_watch");
  }
}

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

  // TODO: use fsnotify for mac
  int inotify_fd = inotify_init();
  if (inotify_fd == -1) {
    err(EXIT_FAILURE, "inotify_init");
  }

  char path[PATH_MAX], *p;
  while (fgets(path, PATH_MAX, stdin)) {
    if (path[0] == '\0') {
      continue;
    }

    // remove newline
    if ((p = strchr(path, '\n')) != NULL) {
      *p = '\0';
    }

    if (is_file_or_dir(path)) {
      // TODO: cannot watch directory RECURSIVELY -> add recursive option
      // make sure there is an upper limit of monitoring fds.
      // you may not watch all directories under the project.
      // TODO: ignore file and dirs in .gitignore
      register_path_to_watch(inotify_fd, path);
    }
  }

  char buffer[BUF_LEN];
  for (;;) {
    if (read(inotify_fd, buffer, BUF_LEN) == -1) {
      err(EXIT_FAILURE, "read");
    }
    run_utility(argv + 1);
  }

  return EXIT_SUCCESS;
}
