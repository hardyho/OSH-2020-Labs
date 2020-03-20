Lab1实验报告
======

linux内核裁剪
-----
根据收集到的资料对linux内核进行裁剪，其中主要裁剪了网络相关部分、设备相关部分、文件系统、各种debug功能等。
裁剪前先利用原始设置完成`initrd`的编写，利用`initrd`对裁剪后的内核进行调试，观察是否有异样。经过裁剪后得到的bzImage大小约为3,900kb，发现执行前会出现菱形问号，但不影响其他功能的使用。

initrd
------
利用了BusyBox部分教程提供的相关知识，编写`init`。其中需要执行提供的三个程序，调用了`mknod`准备`/dev/ttyS0`和`/dev/fb0`，在程序执行结束后使用`while true`,`sleep`指令防止引起kernel panic
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
