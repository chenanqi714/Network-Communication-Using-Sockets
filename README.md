# Network-Communication-Using-Sockets
Utilized Sockets for communication between a client and a server, which demonstrate a message posting system

##complie server
gcc project3-server.c -lpthread -o server

##compile client
gcc project3-client.c -lpthread -o client

##run server (if port number is 3304, hostname is csgrads1.utdallas.edu)
./server 3304

##run client
./client csgrads1.utdallas.edu 3304
