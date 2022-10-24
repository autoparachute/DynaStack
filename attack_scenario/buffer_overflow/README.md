## To compile the overflow.c: ##
	$ su root
	$ sysctl -w kernel.randomize_va_space=0
	$ gcc -g -z execstack -fno-stack-protector overflow.c -o overflow

---
## To find the return_address in exploit.py: ##
	./overflow $(python exploit.py)
	the output address is return_address

---
## To get the shell: ##
	./overflow $(python exploit.py)
