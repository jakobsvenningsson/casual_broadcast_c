CC=gcc
all: ./src/main.c 
	$(CC) -g -Wall -o casual_broadcast ./src/main.c ./src/array.c
clean: 
	rm casual_broadcast
