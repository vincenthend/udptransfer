all : sendfile recvfile

sendfile: ./src/sendFile.c ./src/frame.c
	gcc ./src/sendFile.c ./src/frame.c -o sendfile -lpthread -std=c99
	
recvfile: ./src/receiveFile.c ./src/frame.c
	gcc ./src/receiveFile.c ./src/frame.c -o recvfile