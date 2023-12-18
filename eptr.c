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
#include <getopt.h>
#include <limits.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

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

static void register_path_to_watch(int inotify_fd, const char *path) {
    int watch_descriptor = inotify_add_watch(
            inotify_fd, path, IN_MODIFY | IN_CREATE | IN_DELETE);
    if (watch_descriptor == -1) {
        err(EXIT_FAILURE, "inotify_add_watch");
    }
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

     /* TODO: use fsevent for mac */
    int inotify_fd = inotify_init();
    if (inotify_fd == -1) {
        err(EXIT_FAILURE, "inotify_init");
    }

    char path[PATH_MAX], *p;
    while (fgets(path, PATH_MAX, stdin)) {
        if (path[0] == '\0') {
            continue;
        }

        /* remove newline */
        if ((p = strchr(path, '\n')) != NULL) {
            *p = '\0';
        }

        if (is_file_or_dir(path)) {
            register_path_to_watch(inotify_fd, path);
        }
    }

    if (!postpone_option_enabled) {
        run_utility(argv + optind);
    }
    char buffer[BUF_LEN];
    for (;;) {
        if (read(inotify_fd, buffer, BUF_LEN) == -1) {
            err(EXIT_FAILURE, "read");
        }
        /* NOTE: how it does to pass evented filename thru stdin pipe instead of placeholder? */
        run_utility(argv + optind);
    }

    return EXIT_SUCCESS;
}
