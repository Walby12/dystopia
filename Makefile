run: main
	./main
main:
	gcc -Wall -Wextra -o main main.c
clean:
	rm -f main
