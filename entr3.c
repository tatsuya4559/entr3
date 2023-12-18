#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <err.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>

#ifdef __linux__
#include <sys/inotify.h>
#elif __APPLE__
#include <sys/event.h>
#endif

/* options */
static bool clear_option_enabled = false;
static bool postpone_option_enabled = false;

static void print_usage(char *cmd) {
    fprintf(stderr, "%s [-cp] -- utility\n", cmd);
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
        /* child process */
        if (clear_option_enabled) {
            printf("\033[2J"); /* clear screen */
            printf("\033[3J"); /* clear scrollback buffer */
            printf("\033[H");  /* move cursor to the head of screen */
            fflush(stdout);
        }

        /* set process group for subprocess to be signaled */
        setpgid(0, 0);

        if (execvp(argv[0], argv) == -1) {
            err(EXIT_FAILURE, "execvp");
        }
    }

    /* parent process */
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

static int init_watcher(void) {
#ifdef __linux__
    int fd = inotify_init();
    if (fd == -1) {
        err(EXIT_FAILURE, "inotify_init");
    }
#elif __APPLE__
    int fd = kqueue();
    if (fd == -1) {
        err(EXIT_FAILURE, "kqueue");
    }
#endif
    return fd;
}

static void register_path_to_watch(int watcher_fd, const char *path) {
#ifdef __linux__
    if (inotify_add_watch(watcher_fd, path, IN_MODIFY | IN_CREATE | IN_DELETE) == -1) {
        err(EXIT_FAILURE, "inotify_add_watch");
    }
#elif __APPLE__
    int target_fd = open(path, O_RDONLY);
    if (target_fd == -1) {
        err(EXIT_FAILURE, "open");
    }
    struct kevent event;
    EV_SET(&event, target_fd, EVFILT_VNODE,
            EV_ADD | EV_ENABLE | EV_CLEAR,
            NOTE_DELETE | NOTE_WRITE | NOTE_RENAME | NOTE_ATTRIB,
            0, NULL);
    if (kevent(watcher_fd, &event, 1, NULL, 0, NULL) == -1) {
        err(EXIT_FAILURE, "kevent");
    }
#endif
}

static void receive_watch_event(int watcher_fd) {
#ifdef __linux__
    size_t buf_len = 1024 * (sizeof(struct inotify_event) + 16);
    char buffer[buf_len];
    if (read(watcher_fd, buffer, buf_len) == -1) {
        err(EXIT_FAILURE, "read");
    }
#elif __APPLE__
    struct kevent event;
    if (kevent(watcher_fd, NULL, 0, &event, 1, NULL) == -1) {
        err(EXIT_FAILURE, "kevent");
    }
#endif
}

int main(int argc, char **argv) {
    /* expect file list from a pipe */
    if (isatty(STDIN_FILENO)) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    int opt;
    while ((opt = getopt(argc, argv, "cp")) != -1) {
        switch (opt) {
        case 'c':
            clear_option_enabled = true;
            break;
        case 'p':
            postpone_option_enabled = true;
            break;
        case '?':
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    /* there is not non-option argument */
    if (optind == argc) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    setup_signal_handler();

    int watcher_fd = init_watcher();

    char path[PATH_MAX], abspath[PATH_MAX], *p;
    while (fgets(path, PATH_MAX, stdin)) {
        if (path[0] == '\0') {
            continue;
        }

        /* remove newline */
        if ((p = strchr(path, '\n')) != NULL) {
            *p = '\0';
        }

        if (is_file_or_dir(path)) {
            if (realpath(path, abspath) == NULL) {
                err(EXIT_FAILURE, "realpath");
            }
            register_path_to_watch(watcher_fd, abspath);
        }
    }

    if (!postpone_option_enabled) {
        run_utility(argv + optind);
    }
    for (;;) {
        receive_watch_event(watcher_fd);

        /* NOTE: how it does to pass evented filename thru stdin pipe instead of placeholder? */
        run_utility(argv + optind);
    }

    return EXIT_SUCCESS;
}
