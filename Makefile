cc=gcc

default: sendfile recvfile

all: sendfile recvfile

sendfile: sendFile.o sendingWindow.o frame.o
	$(CC) -o sendfile sendFile.o sendingWindow.o frame.o

recvfile: receiveFile.o frame.o
	$(CC) -o recvfile receiveFile.o frame.o

sendFile.o: ./src/sendFile.c
	$(CC) -c ./src/sendFile.c -std=c99

receiveFile.o: ./src/receiveFile.c
	$(CC) -c ./src/receiveFile.c -std=c99

sendingWindow.o: ./src/sendingWindow.c
	$(CC) -c ./src/sendingWindow.c

frame.o: ./src/frame.c
	$(CC) -c ./src/frame.c

clean:
	rm -f *.o
