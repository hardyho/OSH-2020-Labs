#define _GNU_SOURCE    // Required for enabling clone(2)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> // For wait(2)
#include <sys/wait.h>  // For wait(2)
#include <sched.h>     // For clone(2)
#include <signal.h>    // For SIGCHLD constant
#include <sys/mman.h>  // For mmap(2)
#include <sys/mount.h> 
#include <sys/syscall.h>
#include <sys/stat.h>
#include <limits.h>
#include <cap-ng.h>




const char *usage =
"Usage: %s <directory> <command> [args...]\n"
"\n"
"  Run <directory> as a container and execute <command>.\n";

void error_exit(int code, const char *message) {
    perror(message);
    _exit(code);
}

int container(void *argv) {
    char **args = (char **) argv;
    //mount --make-rprivate
    if(mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0) {
        perror("mount private");
        exit(0);
    }

    char tmpdir[] = "/tmp/lab4-XXXXXX";
    mkdtemp(tmpdir);
    //printf("Tmpdir: %s\n",tmpdir);
    if (mount(".", tmpdir, "ext4", MS_RELATIME | MS_BIND, NULL) < 0) {
        perror("mount rootfs"); exit(0); }  
    

    char oldrootdir[64] = "";
    sprintf(oldrootdir, "%s/oldroot", tmpdir);
    mkdir(oldrootdir, 0777);
    if (syscall(SYS_pivot_root, tmpdir, oldrootdir) < 0) {
        perror("pivot_root"); exit(0); }  

    if (chdir("/") < 0) {
        perror("chdir"); exit(0); }

    char containerdir[64]= "";
    sprintf(containerdir, "/oldroot%s", tmpdir);
    rmdir(containerdir);

    if (umount2("/oldroot", MNT_DETACH) == -1){
        perror("umount2"); exit(0); }
    rmdir("/oldroot");

    //mount
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RELATIME, NULL) < 0) {
        perror("mount proc"); exit(0); }  
    if (mount("sysfs", "/sys", "sysfs", MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RELATIME, NULL) < 0) {
        perror("mount sys"); exit(0); }  
    if (mount("udev", "/dev", "devtmpfs", MS_NOSUID | MS_RELATIME, NULL) < 0) {
        perror("mount dev"); exit(0); }
    if (mount("none", "/tmp", "tmpfs", MS_NOSUID | MS_NODEV | MS_NOATIME, NULL) < 0) {
        perror("mount tmp"); exit(0); }  

    capng_clear(CAPNG_SELECT_BOTH);
    capng_updatev(CAPNG_ADD, CAPNG_EFFECTIVE|CAPNG_PERMITTED, 
                  CAP_SETPCAP, CAP_MKNOD, CAP_AUDIT_WRITE, CAP_CHOWN,
                  CAP_NET_RAW, CAP_DAC_OVERRIDE, CAP_FOWNER, CAP_FSETID,
                  CAP_KILL, CAP_SETUID, CAP_SETGID, CAP_NET_BIND_SERVICE, 
                  CAP_SYS_CHROOT, CAP_SETFCAP, -1);
    capng_apply(CAPNG_SELECT_BOTH);



    execvp(args[0], args);
    error_exit(-1, "exec");
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
                   CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWPID | CLONE_NEWCGROUP | SIGCHLD, argv + 2);

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
