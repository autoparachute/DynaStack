## **缓冲区溢出攻击** ## 
函数内局部变量的内存分配是发生在栈里的，因此，如果某一函数内部声明缓冲区变量，那么，该变量所占用的内存空间是在该函数被调用时所建立的栈。由于对缓冲区变量的一些操作（例如，字符串复制STRCPY）是从低内存地址向高内存地址，而内存中所保存的函数调用的返回地址（RET）通常就位于该缓冲区的高地址，函数的返回地址被覆盖。  
例如函数strcpy()把字符串复制到buffer数组时，数组元素操作顺序为从低地址向高地址，如果字符串长度超过buffer的预留字符长度，超过部分的字符串就会覆盖了EBP,RET返回地址，从而造成缓冲区溢出，使得程序崩溃，若覆盖内容为攻击者精心设计的跳转地址，将导致计算机系统被非法攻击。  

## **Return-to-libc攻击** ## 
① 使用libc库中system函数的地址覆盖掉原本的返回地址；这样原函数返回的时候会转而调用system函数。  
② 设置system函数返回后的地址(filler)，并为system函数构造参数(“/bin/sh”)，最终实现程序执行system(“/bin/sh”)。  
  
## **Return-Oriented-Programing攻击** ##  
ROP攻击利用进程已有的代码片段（称为gdaget），通过栈溢出等漏洞将选定的多个gadget地址与其相关数据结合，实现构造payload的构造，使得进程在执行函数返回时，劫持控制流跳转到指定的gadget，将参数出栈实现栈平衡，再跳转到新的函数执行，从而进行恶意攻击。  