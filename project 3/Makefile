all:
	gcc -c sbmemlib.c -o sbmemlib.o -lpthread -lrt
	ar rcs sbmem.a sbmemlib.o
	gcc -o bin app.c -L. -l:sbmem.a -lpthread -lrt
	./bin