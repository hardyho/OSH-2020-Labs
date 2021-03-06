Lab2 实验报告
===

测试相关
--
1. 文件输出重定向可能需要root权限 

2. TCP重定向测试时需要先在另一终端建立监听，否则无法连接

3. 本程序不允许未设置过的环境变量的使用

实验内容
--


### 管道的实现

为了处理多层管道的情况，显然要创建的子进程数量不是固定的。由于多层管道结构本身具有显而易见的递归性质，在本程序中也使用了递归的思路进行创建。其中主要利用了`pipe()`,`fork()`,`dup2()`,`close()`,`waitpid()`几个函数。

#### 遇到的问题

相对其他部分而言，本次实验中管道功能遇到的bug最多。其中一个bug会使命令的左半部分重复执行多次，并以Segmentation Fault结束。如`ls | wc`执行后会自动执行多次`ls`。由于这个过程甚至跳过了主程序中的输入应该产生的等待，预计与标准输入相关。最终尝试到的解决办法为，在父进程中先存储好STDIN的值，并在进程结束后将其复原。


### 重定向的实现

重定向使用到了`freopen()`，非常方便，该函数直接实现了本部分的主要功能。该部分首先利用`strcmp()`遍历指令寻找重定向符号，并存下其后的文件名。

本程序还实现了TCP重定向，在代码中两个功能在一起完成。具体会在之后TCP重定向的部分进行介绍。

#### 遇到的问题

本部分中，当涉及到重定向输出至文件时，似乎**需要root权限才能正常运作**，否则会产生输出错误。


### Ctrl+C的处理

根据要求，Ctrl+C要能丢弃输入到一半的命令，因此其作用范围显然应从接受指令输入开始，而不能只在执行阶段起作用。通过在`main()`中的大循环内利用`fork()`创建子进程可以实现该要求。要求子进程和父进程在接收到Ctrl+C时做出不同的反应：子进程要退出，父进程不作响应。通过使用`signal()`可以实现该功能。

其中需要特别注意`exit`指令，该指令在原先执行时可以通过返回值等多种方式传递信息，令程序结束循环。但当使用子进程来执行指令时，`exit`的作用往往局限在子进程内，无法对父进程产生影响。本程序中使用`WEXITSTATUS(status)`进行处理，通过将`exit`设置为`exit(1)`，并在父进程监控子进程退出状态，即可实现相应功能。

#### 遇到的问题

在使用Ctrl+C结束shell中运行的程序时，实际没有对所有层次的子进程产生作用。这似乎会导致孤儿进程的产生。为确保不会发生这种情况，在负责实际执行指令的`execute()`部分，调用子进程时增加了`prctl(PR_SET_PDEATHSIG,SIGKILL)`，保证子进程在父进程结束时一起结束。


### TCP重定向的处理 （选）

TCP重定向主要涉及两个部分，一部分是检测字符串`/dev/tcp/`，并从中分离出IP地址和port值。另一部分则是使用`socket()`,`connect()`两个函数建立连接，并使用`dup2()`完成重定向。

要检测该功能需要先在一terminal使用`nc -l <host> <port>`指令建立连接，之后再在另一terminal执行TCP重定向相关的指令。要完成输入重定向，需要在使用`nc`后输入内容，并使用Ctrl+C结束输入。


### 环境变量的处理（选）

通过观察发现环境变量中通常只包含数字、字母、下划线三类字符。当遇到`$`后，会截取符合前述规则的字符串，将其替换为环境变量的值，对其后的部分不做处理，进行拼接。如`$HOME/abcd`会被替换为`root/abcd`（在root权限下）。本程序中利用`getenv()`，按照前述思路进行处理。其效果为：

```
# echo 123$HOME/abcd
123/root/abcd
```

与系统的shell一致。

#### 遇到的问题

当使用到未设置过的环境变量时，系统的shell会将其替换为NULL。由于这样做有可能对指令的处理产生未知的影响，本程序中不允许这种使用，将会报错后直接退出这一指令。


### Strace

```
execve("/bin/sh", ["sh"], 0x7ffd152b9fc0 /* 56 vars */) = 0
brk(NULL)                               = 0x557747083000
access("/etc/ld.so.nohwcap", F_OK)      = -1 ENOENT (No such file or directory)
access("/etc/ld.so.preload", R_OK)      = -1 ENOENT (No such file or directory)
openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3
fstat(3, {st_mode=S_IFREG|0644, st_size=82406, ...}) = 0
mmap(NULL, 82406, PROT_READ, MAP_PRIVATE, 3, 0) = 0x7faf53d7f000
close(3)                                = 0
access("/etc/ld.so.nohwcap", F_OK)      = -1 ENOENT (No such file or directory)
openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libc.so.6", O_RDONLY|O_CLOEXEC) = 3
read(3, "\177ELF\2\1\1\3\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0\260\34\2\0\0\0\0\0"..., 832) = 832
fstat(3, {st_mode=S_IFREG|0755, st_size=2030544, ...}) = 0
mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7faf53d7d000
mmap(NULL, 4131552, PROT_READ|PROT_EXEC, MAP_PRIVATE|MAP_DENYWRITE, 3, 0) = 0x7faf5377c000
mprotect(0x7faf53963000, 2097152, PROT_NONE) = 0
mmap(0x7faf53b63000, 24576, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x1e7000) = 0x7faf53b63000
mmap(0x7faf53b69000, 15072, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) = 0x7faf53b69000
close(3)                                = 0
arch_prctl(ARCH_SET_FS, 0x7faf53d7e540) = 0
mprotect(0x7faf53b63000, 16384, PROT_READ) = 0
mprotect(0x5577467cd000, 8192, PROT_READ) = 0
mprotect(0x7faf53d94000, 4096, PROT_READ) = 0
munmap(0x7faf53d7f000, 82406)           = 0
getuid()                                = 0
getgid()                                = 0
getpid()                                = 13291
rt_sigaction(SIGCHLD, {sa_handler=0x5577465c4200, sa_mask=~[RTMIN RT_1], sa_flags=SA_RESTORER, sa_restorer=0x7faf537baf20}, NULL, 8) = 0
geteuid()                               = 0
brk(NULL)                               = 0x557747083000
brk(0x5577470a4000)                     = 0x5577470a4000
getppid()                               = 13237
stat("/home/pb18000239/OSH-2020-Labs/lab2", {st_mode=S_IFDIR|0755, st_size=4096, ...}) = 0
stat(".", {st_mode=S_IFDIR|0755, st_size=4096, ...}) = 0
ioctl(0, TCGETS, {B38400 opost isig icanon echo ...}) = 0
ioctl(1, TCGETS, {B38400 opost isig icanon echo ...}) = 0
geteuid()                               = 0
getegid()                               = 0
rt_sigaction(SIGINT, NULL, {sa_handler=SIG_DFL, sa_mask=[], sa_flags=0}, 8) = 0
rt_sigaction(SIGINT, {sa_handler=0x5577465c4200, sa_mask=~[RTMIN RT_1], sa_flags=SA_RESTORER, sa_restorer=0x7faf537baf20}, NULL, 8) = 0
rt_sigaction(SIGQUIT, NULL, {sa_handler=SIG_DFL, sa_mask=[], sa_flags=0}, 8) = 0
rt_sigaction(SIGQUIT, {sa_handler=SIG_IGN, sa_mask=~[RTMIN RT_1], sa_flags=SA_RESTORER, sa_restorer=0x7faf537baf20}, NULL, 8) = 0
rt_sigaction(SIGTERM, NULL, {sa_handler=SIG_DFL, sa_mask=[], sa_flags=0}, 8) = 0
rt_sigaction(SIGTERM, {sa_handler=SIG_IGN, sa_mask=~[RTMIN RT_1], sa_flags=SA_RESTORER, sa_restorer=0x7faf537baf20}, NULL, 8) = 0
openat(AT_FDCWD, "/dev/tty", O_RDWR)    = 3
fcntl(3, F_DUPFD, 10)                   = 10
close(3)                                = 0
fcntl(10, F_SETFD, FD_CLOEXEC)          = 0
ioctl(10, TIOCGPGRP, [13237])           = 0
getpgrp()                               = 13237
rt_sigaction(SIGTSTP, NULL, {sa_handler=SIG_DFL, sa_mask=[], sa_flags=0}, 8) = 0
rt_sigaction(SIGTSTP, {sa_handler=SIG_IGN, sa_mask=~[RTMIN RT_1], sa_flags=SA_RESTORER, sa_restorer=0x7faf537baf20}, NULL, 8) = 0
rt_sigaction(SIGTTOU, NULL, {sa_handler=SIG_DFL, sa_mask=[], sa_flags=0}, 8) = 0
rt_sigaction(SIGTTOU, {sa_handler=SIG_IGN, sa_mask=~[RTMIN RT_1], sa_flags=SA_RESTORER, sa_restorer=0x7faf537baf20}, NULL, 8) = 0
rt_sigaction(SIGTTIN, NULL, {sa_handler=SIG_DFL, sa_mask=[], sa_flags=0}, 8) = 0
rt_sigaction(SIGTTIN, {sa_handler=SIG_DFL, sa_mask=~[RTMIN RT_1], sa_flags=SA_RESTORER, sa_restorer=0x7faf537baf20}, NULL, 8) = 0
setpgid(0, 13291)                       = 0
ioctl(10, TIOCSPGRP, [13291])           = 0
wait4(-1, 0x7ffc1e27a6bc, WNOHANG|WSTOPPED, NULL) = -1 ECHILD (No child processes)
stat("/var/mail/root", 0x7ffc1e27a790)  = -1 ENOENT (No such file or directory)
write(2, "# ", 2# )                       = 2
read(0, ^C0x5577467cfa60, 8192)           = ? ERESTARTSYS (To be restarted if SA_RESTART is set)
--- SIGINT {si_signo=SIGINT, si_code=SI_KERNEL} ---
rt_sigprocmask(SIG_SETMASK, [], ~[KILL STOP RTMIN RT_1], 8) = 0
write(2, "\n", 1
)                       = 1
wait4(-1, 0x7ffc1e27a6bc, WNOHANG|WSTOPPED, NULL) = -1 ECHILD (No child processes)
stat("/var/mail/root", 0x7ffc1e27a790)  = -1 ENOENT (No such file or directory)
write(2, "# ", 2# )                       = 2
read(0, 
```
其中用到的系统调用：`brk()`,`openat()`,`fstat()`

- `brk()`改变 "program brek" 的位置，这个位置定义了进程数据段的终止处，即堆部分结束后的第一个位置。 其参数是一个地址，假如已经知道了堆的起始地址，还有堆的大小，那么就可以据此修改 `brk()` 中的地址参数以达到调整堆的目的。

- `openat()`与`open()`相比增加了一个参数 dirfd ，通过这个参数，`openat()`函数允许了 dirfd 不等于 AT_FDCWD 的情况，在这种情况下会以 dirfd 为基础建立相对路径。引入`openat()`使得一个进程内的各线程可拥有不同的当前目录。

- `fstat()`通过文件描述符取得其状态，将参数fildes所指的文件状态，复制到参数buf所指的结构中(struct stat)。
