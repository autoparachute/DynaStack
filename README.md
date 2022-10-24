# Linux操作系统二进制程序安全保护技术 #

---
## 项目介绍 ##
研究项目以32位Linux二进制可执行程序的控制流劫持攻击为对象，从代码注入攻击和代码重用攻击两大控制流劫持攻击分类中选取三类典型攻击手段，提出具有缓冲区溢出漏洞、return-to-libc漏洞、ROP漏洞的三套C/C++程序，构建具有多种类型控制流劫持攻击验证的场景。  
然后，基于开源的DynamoRIO平台设计实现一种影子栈保护机制：当应用软件执行call指令进行函数调用时，该机制将同步保存返回地址，在软件执行ret类指令进行函数返回时，该机制将比较应用软件当前堆栈中的返回地址与之前该机制保存的返回地址是否一致，并据此作为软件是否遭受控制流劫持攻击的判据。  
最后，在DynamoRIO影子栈插件中运行多种类型的控制流劫持漏洞程序，在应用软件执行call指令时备份保存返回地址，最终函数返回时，通过比较当前堆栈返回地址与影子栈备份的返回地址一致性，保护程序控制流安全。证明影子栈机制的Dynamo RIO插件对于控制流劫持攻击的有效防御，验证该机制能够在不需要具体攻击特征的条件下，有效应对多种已知和未知控制流劫持攻击，对二进制可执行程序运行防护具有重要的意义。

---
### **DynamoRIO简介** ###
动态二进制插桩框架DynamoRIO通过对程序代码进行插桩(Instrumentation)执行构建源程序代码与操纵代码之间的桥梁,使DynamoRIO的客户端编写者能够在更高的层面上驾驭原有的程序代码。用户可以透明的修改原有的程序代码,执行追踪,回调,调试,模拟等高级运行时代码操纵(Runtime Code Manipulation)技术。

### **DynamoRIO插桩主要流程** ###
① 入口点分析  
由于执行流控制的原因无法通过对现有进程注入的方式实现插桩，只能通过drrun启动进程挂起获取进程启动的控制权后在入口点实现动态库模块的注入。Drrun调用CreateProcess()后调用inject_into_thread()加载相关动态库。客户端程序执行过程中通过call_switch_stack()不断调用dr_dispatch回调插桩函数来实现整个程序的循环。  

② 插桩循环分析  
dr_dispatch()的主要逻辑是将被插桩程序的当前指令位置eip也就是bb->cur_pc解码成一个个basicblock基本块，并将其中每个汇编指令对应一个指令结构(instruction)并放入instrlist_t容器中，处理每个基本块对应的分支跳转结构(cti)(包括direct_call(程序集内部调用),indirect_call(通过寄存器计算出的地址进行间接调用),cbr(条件跳转),ubr(非条件跳转),mbr(通过寄存器等的间接跳转或调用分支)),将这个容器中的指令编码后生成一个fragment,链接其中的incoming_stubs对应结构linkstub_t中的cti_offset构成了所有块之间的调用顺序,调用dcontext_t中fcache_enter方法跳转到程序运行空间vmarea中的start_pc中执行插桩循环,最后又回到dr_dispatch,这是DynamoRIO主干流程。  

③ 插桩API回调分析  
DynamoRIO提供了drmgr_register_bb_instrumentation_event等api实现了挂钩每个插桩过程的回调，这些回调的入口由call_all_ret宏将参数传给由dr_register_bb_event注册的bb_callbacks回调数组进行分发回调。  
DynamoRIO被插桩的数据块分为2种类型,一种是由每个分支的转移为划分创建的basicblock(基本块),而已经创建过basicblock被多次高频率执行的则以DynamoRIO在代码缓存中作为trace(缓存块)，不重复生成以缓存形式被重复快速调用。DynamoRIO提供了一系列用于指令列表的api用于监视,分析,修改,插入等操作,这些instrlist相对于客户端来说是透明的,客户端可以提供instrlist_first_app()和instr_get_next_app()遍历这些指令列表，其中不包括那些由DynamoRIO生成的label指令,Meta_Instructions(元指令)等。  

---
### **多类型的控制流攻击方式** ###
 
制流劫持攻击通过构造特定攻击载体，利用缓冲区溢出等软件漏洞，非法篡改进程中的控制数据，从而改变进程的控制流程并执行特定的恶意代码，达到攻击目的。根据攻击代码的来源，通常将控制流劫持攻击划分为代码注入类攻击和代码重用类攻击。  
**代码注入类攻击**  
在代码注入攻击中，攻击者通常利用进程的输入操作向被攻击进程的地址空间注入恶意代码，并且通过覆盖函数返回地址等手段，使进程非法执行注入的恶意代码，从而劫持进程控制流达到攻击目的。缓冲区溢出攻击即典型的注入类攻击。  
**代码重用类攻击**  
与代码注入类攻击不同，代码重用攻击攻击不需要攻击者向进程地址空间中注入恶意代码，它仅仅需要利用程序或者共享库中已有的指令来完成攻击，这样的攻击模式的图灵完备性已经得到了证明，故具有危害大，易扩展的特性。面向返回的编程攻击（Return-to-libc、Return-Oriented-Programming）即为典型的代码重用类攻击。

---  

**缓冲区溢出攻击**  
函数内局部变量的内存分配是发生在栈里的，因此，如果某一函数内部声明缓冲区变量，那么，该变量所占用的内存空间是在该函数被调用时所建立的栈。由于对缓冲区变量的一些操作（例如，字符串复制STRCPY）是从低内存地址向高内存地址，而内存中所保存的函数调用的返回地址（RET）通常就位于该缓冲区的高地址，函数的返回地址被覆盖。  
例如函数strcpy()把字符串复制到buffer数组时，数组元素操作顺序为从低地址向高地址，如果字符串长度超过buffer的预留字符长度，超过部分的字符串就会覆盖了EBP,RET返回地址，从而造成缓冲区溢出，使得程序崩溃，若覆盖内容为攻击者精心设计的跳转地址，将导致计算机系统被非法攻击。  

**Return-to-libc攻击**  
① 使用libc库中system函数的地址覆盖掉原本的返回地址；这样原函数返回的时候会转而调用system函数。  
② 设置system函数返回后的地址(filler)，并为system函数构造参数(“/bin/sh”)，最终实现程序执行system(“/bin/sh”)。  
  
**Return-Oriented-Programing攻击**  
ROP攻击利用进程已有的代码片段（称为gdaget），通过栈溢出等漏洞将选定的多个gadget地址与其相关数据结合，实现构造payload的构造，使得进程在执行函数返回时，劫持控制流跳转到指定的gadget，将参数出栈实现栈平衡，再跳转到新的函数执行，从而进行恶意攻击。  
  
---  
### **影子栈介绍** ###
缓冲区溢出攻击、return-to-libc攻击、ROP攻击、等均需对栈空间中的返回地址ret进行修改，劫持控制流，对二进制程序的安全造成威胁。故对栈空间中返回地址ret的有效保护是防御上述攻击的重要思路。  
为了防止在返回地址被覆盖后，程序控制流被劫持跳转至恶意代码处，影子栈机制在数据段创建一块特殊区域用以保存函数返回地址副本，在每次执行ret指令时，系统检查返回地址与地址副本内容是否一致，如果一致，则认为栈上的返回地址未被篡改，否则认为地址被修改，程序报异常并退出。本选题的研究思路是利用开源的DynamoRIO平台提供的API进行拓展插件的开发，构造一个可以拷贝返回地址的影子栈，在系统执行Call函数调用时，影子栈会同步入栈返回地址，当函数返回时，基于DynamoRIO开发的客户端(client)插件会将内存栈中的返回地址与影子栈中备份的返回地址进行比较，若不一致，则认为返回地址被篡改，客户端插件报异常并退出，从而实现对**Linux操作系统二进制程序安全的保护**。  
