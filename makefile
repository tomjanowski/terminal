all: server client
server: server.o
	g++ -o $@ $<
client: client.o
	g++ -o $@ $<
