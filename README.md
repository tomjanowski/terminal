# _Inverted_ server-client openssl terminal

This work in progress is an experiment on openssl programming, while
implementing inverted client-server pair for remote terminal connection.
It is designed to provide similar experience to an ssh session. The
"client" listens for incoming connection, while the "server" attempts to
connect to a fixed IP address once every minute or so, therefore the
client can be executed only on a specific target machine with an IP
address to which the server attempts to connect. Both the client and the
server programs require explicit specification of CA, a certificate and
a private key. They authenticate each other's certificates before
proceeding. It is also required to specify IP addresses and TCP ports
that will be used for communication.

The purpose of this project is to enable access to computers
behind a firewall, or computers that are running on an internal
(private) network with a SNAT router.
