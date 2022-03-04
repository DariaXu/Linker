CC=g++
CFLAGS=-g

linker: linker.cpp
	$(CC) $(CFLAGS) linker.cpp -o linker

clean:
	rm -f linker *.o *~
