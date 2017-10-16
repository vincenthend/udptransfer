#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include "frame.h"

#define PORT 8000

void handleError(char* msg);
off_t fsize(const char* fileName);
void* sendFile();
void* receiveAck();
void* timeout();

// Global variables
int my_port, my_ipadr, my_sock;
int dest_port, dest_ipadr, dest_sock;
int recv_len;
int windowSize, bufferSize;
off_t fileSize;
struct sockaddr_in socket_sender, socket_destination;
char msg[6];
FILE* fptr;
char* fileName;
char* senderBuffer;
Segment* sentSegment;
Segment** windowBuffer;
ACK* ack;

// Runtime variables
int lfs, lar, windowBufferPtr;
uint32_t seqnum;
char finish = 0;

int main (int argc, char* argv[]) {
	pthread_t tidSend, tidReceiveAck, tidTimeout;

	if (argc != 6) {
		handleError("Usage: ./sendfile <filename> <windowsize> <buffersize> <destination_ip> <destination_port>\n");
	}

	////////////////////
	// Initialization //
	////////////////////
	
	// Set port used to send
	my_port = PORT;
	
	// Read arguments
	fileName = argv[1];
	sscanf(argv[2], "%d", &windowSize);
	sscanf(argv[3], "%d", &bufferSize);
	dest_ipadr = inet_addr(argv[4]);
	sscanf(argv[5], "%d", &dest_port);

	// Open file
	fptr = fopen(fileName, "rb");
	fileSize = fsize(fileName);
	if (!fptr || fileSize == -1) {
		handleError("Error: Unable to open file\n");
	}	

	// Set buffer	
	senderBuffer = (char*) malloc(bufferSize * sizeof(char));
	windowBuffer = (Segment**) malloc(windowSize * sizeof(Segment*));

	// Init runtime vars
	lfs = -1;
	lar = -1;
	seqnum = 0;

	// Error checking of arguments
	
	/////////////////////
	// Socket Creation //
	/////////////////////
	
	// Initialize socket
	my_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (my_sock == -1) {
		handleError("Error: Failed to initialize socket\n");
	}
	
	socket_sender.sin_family = AF_INET;
	socket_sender.sin_port = htons(my_port);
	socket_sender.sin_addr.s_addr = htonl(INADDR_ANY);

	// Binding socket address to port
	if (bind(my_sock, (struct sockaddr*)&socket_sender, sizeof(socket_sender)) == -1){
		handleError("Error: Failed to bind socket\n");
	}
	
	///////////////////////
	//  Segment Sending  //
	///////////////////////

	// Setup Sending Address
	socket_destination.sin_family = AF_INET;
	socket_destination.sin_port = htons(dest_port);
	socket_destination.sin_addr.s_addr = dest_ipadr;

	printf("Sending to %s\n	", inet_ntoa(socket_destination.sin_addr));	
	
	// Send data
	pthread_create(&tidSend, NULL, sendFile, NULL);
	pthread_create(&tidReceiveAck, NULL, receiveAck, NULL);
	pthread_create(&tidTimeout, NULL, timeout, NULL);

	pthread_join(tidSend, NULL);
	pthread_join(tidReceiveAck, NULL);

	fclose(fptr);
	return 0;
}






void handleError(char* msg) {
	fprintf(stderr, "%s", msg);
	exit(1);
}

off_t fsize(const char* fileName) {
	struct stat st;

	if (stat(fileName, &st) == 0) {
		return st.st_size;
	} else {
		return -1;
	}
}

void* sendFile() {
	while (!feof(fptr)) {
		// Read file
		fread(senderBuffer, bufferSize, 1, fptr);

		windowBufferPtr = 0;
		for (int i = 0; i < bufferSize && seqnum < fileSize; ++i) {
			// Convert data to segment
			sentSegment = (Segment*) malloc(sizeof(Segment));
			initSegment(sentSegment, seqnum, senderBuffer[i]);
			seqnum++;

			// Fill sender buffer
			windowBuffer[windowBufferPtr] = sentSegment;
			windowBufferPtr = (windowBufferPtr + 1) % windowSize;

			// Send data
			if (sendto(my_sock, sentSegment, sizeof(sentSegment), 0, (struct sockaddr*) &socket_destination,sizeof(socket_destination)) == -1){
				handleError("Error: Failed to send data\n");
			}
			lfs++;

			// Wait for ACK
			while (!(lfs - lar <= windowSize));
		}
	}

	pthread_exit(NULL);
}

void* receiveAck() {


	pthread_exit(NULL);
}

void* timeout() {
	pthread_exit(NULL);
}

// Glosarium
/*
	INADDR_ANY --> konstanta untuk any / 0.0.0.0
	socket(domain, type, protocol) --> membuat socket
	bind(socket, sockaddress, address_length) --> binding socket dengan port
	
	sockaddr_in --> struktur data sockaddress buat internet (ada sin_family (AF_INET --> address family), sin_port (port), sin_addr (IP address))
	sendto(socket, message, message_length, flags, destination, destination_length) --> mengirim packet
	recvfrom(socket, buffer, length, flags, address, length)
	
	
	htons = host to network byte order (short)
	htonl = host to network byte order (long)
	
*/