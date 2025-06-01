all: client server

client: client.c cJSON.c cJSON.h
	gcc -o client client.c cJSON.c

server: server.c cJSON.c cJSON.h
	gcc -o server server.c cJSON.c

clean:
	rm -f client server
