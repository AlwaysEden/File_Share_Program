CC = gcc
CFLAGS = -Wall
TARGETS = server client


all: $(TARGETS)

server: server.c
	$(CC) $(CFLAGS) -o server server.c -pthread

client: client.c
	$(CC) $(CFLAGS) -o client client.c -pthread
debug:
	$(CC) $(CFLAGS) -o dserver server.c -pthread -DDEBUG
	$(CC) $(CFLAGS) -o dclient client.c -pthread -DDEBUG

clean:
	rm -f $(TARGETS)
