#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <err.h>
#include <sys/epoll.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>

#define MAX_EVENTS 10

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

static int epoll_fd;

static void register_fd(int epfd, int fd) {
  struct epoll_event event;
  // Oh... regular file does not support EPOLLIN event.
  // because regular files are always readable.
  event.events = EPOLLIN;
  event.data.fd = fd;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event) == -1) {
    err(EXIT_FAILURE, "epoll_ctl");
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

  // Create a new epoll instance.
  // This will be destroyed and released
  // when all monitoring files have been closed.
  if ((epoll_fd = epoll_create1(0)) == -1) {
    err(EXIT_FAILURE, "epoll_create1");
  }

  char buf[PATH_MAX];
  char *p;
  struct stat file_info;
  int fd;
  while (fgets(buf, PATH_MAX, stdin)) {
    if (buf[0] == '\0') {
      continue;
    }

    // remove newline
    if ((p = strchr(buf, '\n')) != NULL) {
      *p = '\0';
    }

    if (stat(buf, &file_info) == -1) {
      fprintf(stderr, "Cannot check stat on %s\n", buf);
      continue;
    }
    if (!S_ISREG(file_info.st_mode)) {
      // monitor regular file only
      continue;
    }

    fd = open(buf, O_RDONLY);
    register_fd(epoll_fd, fd);
  }

  struct epoll_event events[MAX_EVENTS];
  for (;;) {
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    if (nfds == -1) {
      err(EXIT_FAILURE, "epoll_wait");
    }
    printf("changed\n");
    /* run_utility(); */
  }

  return EXIT_SUCCESS;
}
