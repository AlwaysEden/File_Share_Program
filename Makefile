CC = gcc
CFLAGS = -Wall
TARGETS = server client


all: $(TARGETS)

server: dirup_server.c
	$(CC) $(CFLAGS) -o $@ $^ -pthread

client: dirup_client.c
	$(CC) $(CFLAGS) -o $@ &^ -pthread
debug:
	$(CC) $(CFLAGS) -o dserver dirup_server.c -pthread -DDEBUG
	$(CC) $(CFLAGS) -o dclient dirup_client.c -pthread -DDEBUG

clean:
	rm $(TARGETS)
