#define _GNU_SOURCE    // Required for enabling clone(2)
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h> // For wait(2)
#include <sys/wait.h>  // For wait(2)
#include <sched.h>     // For clone(2)
#include <signal.h>    // For SIGCHLD constant
#include <sys/mman.h>  // For mmap(2)


const char *usage =
"Usage: %s <directory> <command> [args...]\n"
"\n"
"  Run <directory> as a container and execute <command>.\n";

void error_exit(int code, const char *message) {
    perror(message);
    _exit(code);
}

int container(void *argv) {
    char *args[2];
    if (chroot(".") == -1)
        error_exit(1, "chroot");
    args[0] = (char *) argv;
    args[1] = NULL;
    execvp(args[0], args);
    error_exit(255, "exec");
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, usage, argv[0]);
        return 1;
    }
    if (chdir(argv[1]) == -1)
        error_exit(1, argv[1]);

    #define STACK_SIZE (1024 * 1024) // 1 MiB
    void *child_stack = mmap(NULL, STACK_SIZE,
                             PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                             -1, 0);
    // Assume stack grows downwards
    void *child_stack_start = child_stack + STACK_SIZE;

    int ch = clone(container, child_stack_start,
                   CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWPID | CLONE_NEWCGROUP | SIGCHLD, argv[2]);

    // Parent waits for child
    int status, ecode = 0;
    wait(&status);
    if (WIFEXITED(status)) {
        printf("Exited with status %d\n", WEXITSTATUS(status));
        ecode = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        printf("Killed by signal %d\n", WTERMSIG(status));
        ecode = -WTERMSIG(status);
    }
    return ecode;
}
