epoll:epoll.c
	gcc -o $@ $^
.PHONY:clean
clean:
	rm -f epoll
