CC = gcc
CFLAGS = -I"C:/msys64/mingw64/include" -L"C:/msys64/mingw64/lib" -static
LIBS = -ljson-c -lws2_32

all: servidor.exe

servidor.exe: servidor.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)