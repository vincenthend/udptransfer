all : sendfile recvfile

sendfile: sendFile.c frame.c
	gcc sendFile.c frame.c -o sendfile -lpthread -std=c99
	
recvfile: receiveFile.c frame.c
	gcc receiveFile.c frame.c -o recvfile