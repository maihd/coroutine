test:
	@echo Compile test program 
	@gcc -o a Coroutine_Test.c Coroutine.c

	@echo
	@echo Execute test program
	@echo ====================
	@./a

	@echo
	@echo Remove test program
	@rm a