CC=gcc
CFLAGS=-Wall -Wextra -O0

ifeq ($(OS),Windows_NT)
CC=clang
CFLAGS+=-D_WIN32
endif

test:
	@echo Testing system native version (Standard C only)

	@echo Compile test program 
	@$(CC) -o a.exe Coroutine_Test.c Coroutine.c $(CFLAGS)

	@echo
	@echo Execute test program
	@echo ====================
	@./a.exe

	@echo
	@echo Remove test program
	@rm -rf a.exe

setjmp:
	@echo "Testing setjmp version (Standard C only)"

	@echo Compile test program 
	@$(CC) -o a.exe setjmp_test.c $(CFLAGS)

	@echo
	@echo Execute test program
	@echo ====================
	@./a.exe

	@echo
	@echo Remove test program
	@rm -rf a.exe