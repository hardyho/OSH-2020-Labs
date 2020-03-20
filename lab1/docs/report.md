#Lab1实验报告

##linux内核裁剪
根据收集到的资料对linux内核进行裁剪，其中主要裁剪了网络相关部分、设备相关部分、文件系统、各种debug功能等。
裁剪前先利用原始设置完成'initrd'的编写，利用'initrd'对裁剪后的内核进行调试，观察是否有异样。经过裁剪后得到的bzImage大小约为3,900kb，发现执行前会出现菱形问号，但不影响其他功能的使用。
  
##initrd
利用了BusyBox部分教程提供的相关知识，编写'init'。其中需要执行提供的三个程序，调用了'mknod'准备'/dev/ttyS0'和'/dev/fb0'，在程序执行结束后使用'while true','sleep'指令防止引起kernel panic
'''    
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

'''
