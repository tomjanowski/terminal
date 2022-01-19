all: server client
server: server.o
	g++ -o $@ $< -lssl -lcrypto
client: client.o
	g++ -o $@ $< -lssl -lcrypto
