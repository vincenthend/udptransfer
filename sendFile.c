#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include "frame.h"

#define PORT 8000
#define TIMEOUT 10

//////////////////////////////////
// Function Forward Declaration //
//////////////////////////////////

/**
 * Displays error message to stderr, then exits program with exitcode 1
 *
 * @param msg Error message
 */
void handleError(const char* msg);

/**
 * Calculates size of file with path fileName
 *
 * @param fileName Path to file
 */
off_t fsize(const char* fileName);

/**
 * Sends file to receiver using the Sliding Window protocol
 */
void* sendFile();

/**
 * Accepts acknowledgement from receiver, then advances LAR accordingly
 */
void* receiveAck();

/**
 * Resets status of frames sent but not yet acknowledged
 *
 * Note: statusTable can have three values: -1, 0, 1
 * -1: frame loaded to buffer, not yet sent or timed out
 * 0: frame sent
 * 1: frame acknowledged
 */
void* timeout();

//////////////////////////////////
// Global Variables Declaration //
//////////////////////////////////

int my_port, my_ipadr, my_sock;
int slen;
int windowSize, bufferSize;
off_t fileSize;
struct sockaddr_in socket_sender, socket_destination;
FILE* fptr;
char* senderBuffer;
char* statusTable;
Segment** windowBuffer;

// Runtime variables
int windowBufferPtr;
uint32_t lfs, lar;
uint32_t seqnum;


int main (int argc, char* argv[]) {
	int dest_port, dest_ipadr, dest_sock;
	char* fileName;
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
	statusTable = (char*) malloc(windowSize * sizeof(char));

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
	slen = sizeof(socket_destination);

	printf("Sending to %s\n	", inet_ntoa(socket_destination.sin_addr));	
	
	// Send data
	pthread_create(&tidSend, NULL, sendFile, NULL);
	pthread_create(&tidReceiveAck, NULL, receiveAck, NULL);
	pthread_create(&tidTimeout, NULL, timeout, NULL);

	pthread_join(tidSend, NULL);
	pthread_join(tidReceiveAck, NULL);
	pthread_join(tidTimeout, NULL);

	fclose(fptr);
	return 0;
}

///////////////////////////////
//  Function Implementation  //
///////////////////////////////

void handleError(const char* msg) {
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
	int sent_len;
	Segment* sentSegment;
	char finish = 0;

	while (!feof(fptr)) {
		// Read file
		fread(senderBuffer, bufferSize, 1, fptr);

		// Initialize status table
		for (int i = 0; i < windowSize; ++i) {
			statusTable[i] == 1;
		}

		windowBufferPtr = 0;
		int i = 0;
		while (!finish) {
			// Convert data to segment
			if (statusTable[windowBufferPtr] == 1 && i < bufferSize && seqnum < fileSize) {
				sentSegment = (Segment*) malloc(sizeof(Segment));
				initSegment(sentSegment, seqnum, senderBuffer[i]);
				seqnum++;
				lfs++;
				i++;

				// Fill sender buffer
				windowBuffer[windowBufferPtr] = sentSegment;
				statusTable[windowBufferPtr] = -1;
			}

			// Send data
			for (int j = 0; j < windowSize; ++j) {
				if (statusTable[j] == -1) {
					sentSegment = windowBuffer[j];
					sent_len = sendto(my_sock, sentSegment, sizeof(sentSegment), 0, (struct sockaddr*) &socket_destination,sizeof(socket_destination));
					if (sent_len == -1) {
						handleError("Error: Failed to send data\n");
					}
					printf("Data %d sent", seqnum - 1);
					statusTable[j] = 0;
				}
			}

			// Advance if smaller than window size and there's still data to send
			if (lfs - lar <= windowSize && i < bufferSize && seqnum < fileSize) {
				windowBufferPtr = (windowBufferPtr + 1) % windowSize;
			}

			// Finish condition: all data sent successfully
			finish = 1;
			for (int j = 0; j < windowSize; ++j) {
				finish &= (statusTable[j] == 1);
			}
			finish |= (lar == fileSize - 1);
		}
	}

	pthread_exit(NULL);
}

void* receiveAck() {
	int recv_len;
	ACK* ack;

	while (lar < fileSize - 1) {
		// Receive ACK
		recv_len = recvfrom(my_sock, (char*) ack, 7, 0, (struct sockaddr*) &socket_destination, &slen);
		if (recv_len != -1) {
			// Check if ACK is in order
			statusTable[(ack->nextseq - 1) % windowSize] = 1;
			if (ack->nextseq == lar + 2) {
				while (statusTable[(lar + 1) % windowSize] == 1)
					lar++;
			}
		}
	}

	pthread_exit(NULL);
}

void* timeout() {
	while (lar < fileSize - 1) {
		sleep(TIMEOUT);
		for (int i = 0; i < windowSize; ++i) {
			if (statusTable[i] == 0)
				statusTable[i] = -1;
		}
	}

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