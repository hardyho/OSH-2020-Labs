Lab1实验报告
======

linux内核裁剪
-----
根据收集到的资料对linux内核进行裁剪，其中主要裁剪了网络相关部分、设备相关部分、文件系统、各种debug功能等。相较general setup而言，这些部分似乎占用了内核中较大的空间。由于裁剪后的内核只用执行几个固定程序，其实很多教程中建议保留的部分也可以直接裁掉，不用一项一项慢慢研究。

裁剪前先利用原始设置完成`initrd`的编写，利用`initrd`对裁剪后的内核进行调试，观察是否有异样。经过裁剪后得到的bzImage大小约为3,900kb，在最后一次裁剪之后发现执行前会出现菱形问号，但不影响其他功能的使用。

initrd
------
利用了BusyBox部分教程提供的相关知识，编写`init`。其中需要执行提供的三个程序，调用了`mknod`准备`/dev/ttyS0`和`/dev/fb0`，在程序执行结束后使用`while true`,`sleep`指令防止引起kernel panic。使用这种方法不会引起控制台花屏。注意到如果使用`qemu`指令时如果没有加上`-serial stdio`部分，三个程序无法正常执行。
```
#!/bin/sh
echo "hello Linux! "
./1
mknod dev/ttyS0 c 4 64
mknod dev/fb0 c 29 0
./2
./3
while true
do
sleep 100
done

```

bare-metal程序
----
提示中给出了利用`int 0x10`进行字符串输出的一小段程序，提到了8254计时器并给出其映射的内存地址。利用polling的思路，通过不断轮询`0x046c`，当数据发生改变时令一计数器自增。当该值增至18时为一秒时间，调用字符串输出部分进行输出并将计数器清零。

```
[BITS 16]
[ORG 0x7c00]

mov ax,3  #实现清屏功能
int 10h

and cx,0  #cx为程序使用的计数器
and EAX,0 #EAX用于polling 
sti
timer:
        mov EBX,EAX   
        mov EAX,[0x046c]
        cmp EBX,EAX   
        je  timer   #如果EAX发生改变 向下执行
        add cx,1 
        cmp cx,18
        je print    #当计数器值为18 转至输出部分
        jmp timer
print:
        and cx,0    #将cx置零 下面部分直接使用给出的输出程序片段
        mov si,OSH
print_str:
        lodsb
        cmp al,0
        je end
        mov ah,0x0e
        int 0x10
        jmp print_str
end:
        mov ah,2    #输出换行符 并回到polling模式
        int 21h
        jmp timer

OSH db 'Hello, OSH 2020 Lab1!' ,0dh,0ah

TIMES 510 - ($ -$$) db 0
DW 0xAA55
```

思考题
--
>在使用 `make menuconfig` 调整 Linux 编译选项的时候，你会看到有些选项是使用 `[M]`标记的。它们是什么意思？在你的 `init` 启动之前的整个流程中它们会被加载吗？如果不，在正常的 Linux 系统中，它们是怎么被加载的？

A:调整编译选项中，有一些项目具有`[*]`/`[ ]`/`[M]`三种选择。选择`[M]`意味着该模块虽然会被编译，但不会被加载到内核中，而是生成后缀为`.o`的文件以备之后进行动态加载。`init`启动之前的过程中这些部分不会被加载。在之后的使用中，调用`insmod`可以动态加载该部分。

>在介绍 BusyBox 的一节，我们发现 `init` 程序可以是一段第一行是 `#!/bin/sh` 的 shell 脚本。尝试解释为什么它可以作为系统第一个启动的程序，并且说明这样做需要什么条件。

A:`#!`是一个标识符，`/bin/sh`则是用于解释脚本的shell的解释器路径。其含义为运行相应的解释器，并执行shell脚本的内容。要这样运行需要将busybox以及对应的解释器打包在initrd中。

>MBR 能够用于编程的部分只有 510 字节，而目前的系统引导器（如 GRUB2）可以实现很多复杂的功能：如从不同的文件系统中读取文件、引导不同的系统启动、提供美观的引导选择界面等，用 510 字节显然是无法做到这些功能的。它们是怎么做到的？

A:MBR是指磁盘最前的一段引导程序，位于硬盘的第一个扇区。在系统加电并完成基本初始化后，控制器交给MBR。GRUB作为启动程序通常占用的体积大于510字节，因此实际上GRUB的引导分为了两个阶段。第一个阶段占用446字节，用汇编编写存储在MBR中，即引导程序部分。这个过程会完成硬件设备的初始化，设置堆栈，并为Bootlaoder的下一阶段进行准备。在完成后将跳转到第二阶段，由c语言编写的程序入口。该部分可以实现问题中提到的功能。在两阶段中间，有些引导程序（GRUB）还会设置stage1.5，在这一部分构造了一个简单的文件系统，并由此找到stage2。

>目前，越来越多的 PC 使用 UEFI 启动。请简述使用 UEFI 时系统启动的流程，并与传统 BIOS 的启动流程比较。

UEFI启动主要分为几个阶段：<br/>
1. SEC 安全验证 接受系统的启动/异常信号，在cache上开辟一段内存使用，给下一阶段传递相关参数<br/>
2. PEI EFI前期的初始化 主要完成CPU和内存等硬件的初始化<br/>
3. DXE 驱动执行环境 对各个Driver进行遍历/初始化<br/>
4. BDS 启动设备选择 初始化控制台设备，根据用户的选择加载相应的设备驱动<br/>
5. TSL 操作系统前期 启动服务结束，为之后的操作系统接管进行准备<br/>
相比传统BIOS减少了自检过程，从而加快了启动的速度，并且没有使用BIOS中大量使用的中断调用，从而提高了启动效率。<br/>