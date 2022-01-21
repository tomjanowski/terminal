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

While the client listens and the server initiates the connection, once
the connection is established the client plays the same role as an ssh
client, i.e. it is the _interactive_ end of the session, where a
person types stuff on their terminal, while the server acts as an sshd
server, where it creates a pseudoterminal with a /bin/bash in an
interactive mode. The server does not use any PAM modules, it simply
creates an new subprocess for handling the interactive session.

The purpose of this project is to enable access to computers
behind a firewall, or computers that are running on an internal
(private) network with a SNAT router.

Things that are implemented:
1. TLS security with mutual certificate verification
2. Command mode, which enables out of band messages to be sent to the
server. Currently used for dynamic terminal resize.
3. KEEPALIVE declared on the server, otherwise the sessions keep
freezing
4. Hopefully a hung session would result in closing the slave end of
the connection and closing the master-serving subprocess, but this is
yet to be seen.

TODO:
1. Exporting env variables, in particular XTERM.
