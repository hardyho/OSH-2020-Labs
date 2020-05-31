# Lab4 实验报告

可能需要在root权限下运行本次实验的程序。

由于本次实验大部分内容在指引中已经有提示，下面按照实验指引的顺序进行简单介绍。



## Namespaces隔离

```
    int ch = clone(container, child_stack_start,
                   CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWPID | SIGCHLD, argv + 2);
```

在`main`中使用`clone`进行命名空间隔离。其中，cgroup的隔离在容器子进程内使用`unshare`进行隔离。




## Mount

在容器内进行挂载。通过使用`strace`，可以找到相关系统调用的具体使用方法。

```
    if(mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0) {
        perror("mount private");
        exit(0);
    }
```

先在容器内递归挂载为私有。

```
    //mount
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RELATIME, NULL) < 0) {
        perror("mount proc"); exit(0); }  
    if (mount("sysfs", "/sys", "sysfs", MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RELATIME, NULL) < 0) {   
        perror("mount sys"); exit(0); }          
    if (mount("udev", "/dev", "devtmpfs", MS_NOSUID | MS_RELATIME, NULL) < 0) {
        perror("mount dev"); exit(0); }
    if (mount("none", "/tmp", "tmpfs", MS_NOSUID | MS_NODEV | MS_NOATIME, NULL) < 0) {
        perror("mount tmp"); exit(0); } 
```

在主机上命令行中使用`mount`观察具体参数，并相应的在容器中进行设置。

由于各个设备节点似乎并没有被隔离，本次没有重新创建`/dev/null`等节点。写这部分代码的时候，需要注意与`chroot`的先后关系，如果顺序不对可能产生奇怪的错误。（与`pivot_root`的顺序是否会造成影响没有进一步测试）

需要挂载`/proc`后才能在容器中使用`mount`，但需要确保已经隔离并且挂载为私有，否则有影响主机的风险。



## Pivot_root 

```
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
 ```

本部分可以参考`pivot_root`的文档，最末处有相关的示例。这里直接使用`rmdir(containerdir)`，在容器内移除了主机中的`/tmp/lab4-XXX`，而没用到的进程通信的方法。

需要注意理清逻辑。最初由于没有完全理解，直接将`/`挂载到`/tmp/lab4-XXX`上，误以为挂载的是容器根目录，造成了一些混乱。



##  Capabilities

该部分较简单，需要注意设置`CAPNG_BOUNDING_SET`，否则在执行`execvp`后无法保留这些能力。可以使用`capsh --print`在容器内查看能力。
```
    capng_clear(CAPNG_SELECT_BOTH);
    capng_updatev(CAPNG_ADD, CAPNG_BOUNDING_SET | CAPNG_EFFECTIVE | CAPNG_PERMITTED, 
                  CAP_SETPCAP, CAP_MKNOD, CAP_AUDIT_WRITE, CAP_CHOWN,
                  CAP_NET_RAW, CAP_DAC_OVERRIDE, CAP_FOWNER, CAP_FSETID,
                  CAP_KILL, CAP_SETUID, CAP_SETGID, CAP_NET_BIND_SERVICE, 
                  CAP_SYS_CHROOT, CAP_SETFCAP, -1);
    capng_apply(CAPNG_SELECT_BOTH);
 ```



## 系统调用过滤

该部分最大的问题应该在于函数`seccomp_rule_add`中需要用到系统调用号，在标准示例中，会用到宏`SCMP_SYS()`来求得某系统调用的调用号。但是在这个宏使用方法如`SCMP_SYS(chdir)`，而非`SCMP_SYS("chdir")`，即并不能使用字符串型。这似乎导致没有办法使用某个文件统一存储要允许的系统调用的名字，并在程序中进行处理。本次实验中直接对每一个系统调用使用了一次`seccomp_rule_add`，似乎不太优雅。
```
    if ((ctx = seccomp_init(SCMP_ACT_KILL)) == NULL) {
        perror("seccomp_init"); exit(0); }
    
    if (seccomp_set() < 0){
        perror("seccomp_set"); exit(0); }
    
    if (seccomp_load(ctx) < 0){
        perror("seccomp_load"); exit(0); }
```
其中`seccomp_set`中将这次涉及到的300余个系统调用加入白名单。

此外，还在`main`中等待容器子进程返回后使用了`seccomp_release`。

为避免不可预测的情况发生，关于capabilities和系统调用的限制最好在最后进行。



## cgroup

```
    //mount cgroup     
    if (mount("tmpfs", "/sys/fs/cgroup", "tmpfs",  MS_NOSUID | MS_NODEV, NULL) < 0) {
        perror("mount tmp"); exit(0); } 
    mkdir("/sys/fs/cgroup/memory", 0777); 
    if (mount("memory", "/sys/fs/cgroup/memory", "cgroup", MS_MGC_VAL, "memory") < 0) {
        perror("mount cgroup/memory"); exit(0); }
    mkdir("/sys/fs/cgroup/cpu,cpuacct", 0777); 
    if (mount("cpu,cpuacct", "/sys/fs/cgroup/cpu,cpuacct", "cgroup", MS_MGC_VAL, "cpu,cpuacct") < 0) {
        perror("mount cgroup/cpu"); exit(0); }
    mkdir("/sys/fs/cgroup/pids", 0777); 
    if (mount("pids", "/sys/fs/cgroup/pids", "cgroup", MS_MGC_VAL, "pids") < 0) {
        perror("mount cgroup/pids"); exit(0); }
```

如图，在容器内挂载cgroup控制器。

完成挂载后，使用`mkdir`，`fopen`，`fprintf`等完成相关限制的设置，完成后使用`unshare`隔离命名空间。

之后进入容器，发现cgroup的根目录与要求不符。

查询 [cgroup_namespaces](https://www.man7.org/linux/man-pages/man7/cgroup_namespaces.7.html) 的文档，找到其中有一个相关的解决方法。

> ​       The fourth field of this line (/..)  should show the directory in the
> ​       cgroup filesystem which forms the root of this mount.  Since by the
> ​       definition of cgroup namespaces, the process's current freezer cgroup
> ​       directory became its root freezer cgroup directory, we should see '/'
> ​       in this field.  The problem here is that we are seeing a mount entry
> ​       for the cgroup filesystem corresponding to the initial cgroup names‐
> ​       pace (whose cgroup filesystem is indeed rooted at the parent direc‐
> ​       tory of sub).  To fix this problem, we must remount the freezer
> ​       cgroup filesystem from the new shell (i.e., perform the mount from a
> ​       process that is in the new cgroup namespace), after which we see the
> ​       expected results:
>
> ​           sh2# mount --make-rslave /     # Don't propagate mount events to other namespaces
>
> ​           sh2# umount /sys/fs/cgroup/freezer
> ​           sh2# mount -t cgroup -o freezer freezer /sys/fs/cgroup/freezer
> ​           sh2# cat /proc/self/mountinfo | grep freezer
> ​           155 145 0:32 / /sys/fs/cgroup/freezer rw,relatime ...

即，在隔离命名空间后重新挂载cgroup控制器，即可解决根目录的问题。尝试后发现可行。

似乎使用这种方法并不会对主机产生任何影响，因而不用在结束后移除控制组（建立的控制组位于容器的`/sys/fs/cgroup`下，每次运行都会重新挂载）。

经检验，这种方法可以完成相关的资源限制。



## 思考题

1. 用于限制进程能够进行的系统调用的 seccomp 模块实际使用的系统调用是哪个？用于控制进程能力的 capabilities 实际使用的系统调用是哪个？尝试说明为什么本文最上面认为「该系统调用非常复杂」。

   

   seccomp使用的是`int seccomp(unsigned int operation, unsigned int flags, void *args);`

   capabilities限制上，使用的系统调用较多。包括

   - `int capget(cap_user_header_t header, cap_user_data_t data);`

   - `int capset(cap_user_header_t header, const cap_user_data_t data);`

   - `prctl(PR_CAPBSET_DROP, CAPNAME);`

   除此之外，capabilities的限制还涉及一些文件的访问。该部分使用的系统调用数量远多于其他部分。

   

2. 当你用 cgroup 限制了容器中的 CPU 与内存等资源后，容器中的所有进程都不能够超额使用资源，但是诸如 htop 等「任务管理器」类的工具仍然会显示主机上的全部 CPU 和内存（尽管无法使用）。查找资料，说明原因，尝试**提出**一种解决方案，使任务管理器一类的程序能够正确显示被限制后的可用 CPU 和内存（不要求实现）。



​		容器内的隔离并不彻底，容器内的`/proc`下仍可看到主机的proc资源信息。而`top`等指令正是通过访问这些文件来获取相关信息的。在网络上可以找到LXCFS项目解决该问题。

​		或许可以将容器内的相关文件内容更改为容器本身的资源数值，从而解决该问题。
