fork_demo: fork_demo.c
	gcc fork_demo.c -o fork_demo
exec_demo: exec_demo.c
	gcc exec_demo.c -o exec_demo
clone_demo: clone_demo.c
	gcc clone_demo.c -o clone_demo -lcgroup
lxc_demo: lxc_demo.c
	gcc lxc_demo.c -o lxc_demo -lcgroup
.PHONY: clean
clean:
	rm lxc_demo fork_demo exec_demo clone_demo
