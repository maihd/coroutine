ifeq ($(OS),Windows_NT)
CC=clang
RM=del /Q
else
CC=gcc
RM=rm -f
endif

test:
	@echo Compile test program 
	@$(CC) -o a Coroutine_Test.c Coroutine.c

	@echo
	@echo Execute test program
	@echo ====================
	@./a

	@echo
	@echo Remove test program
	@$(RM) a