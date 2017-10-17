#define _BSD_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
void handleError(const char *msg);

/**
 * Calculates size of file with path fileName
 *
 * @param fileName Path to file
 */
off_t fsize(const char *fileName);

/**
 * Sends file to receiver using the Sliding Window protocol
 */
void *sendFile();

/**
 * Accepts acknowledgement from receiver, then advances LAR accordingly
 */
void *receiveAck();

/**
 * Resets status of frames sent but not yet acknowledged
 *
 * Note: statusTable can have three values: -1, 0, 1
 * -1: frame loaded to buffer, not yet sent or timed out
 * 0: frame sent
 * 1: frame acknowledged
 */
void *timeout();

//////////////////////////////////
// Global Variables Declaration //
//////////////////////////////////

int my_port, my_ipadr, my_sock;
int slen;
int windowSize, bufferSize;
off_t fileSize;
struct sockaddr_in socket_sender, socket_destination;
FILE *fptr;
char *senderBuffer;
char *statusTable;
char *timeoutTable;
Segment **windowBuffer;
char status;

// Runtime variables
int windowBufferPtr;
uint32_t lfs, lar;
uint32_t seqnum;

int main(int argc, char *argv[]) {
	int dest_port, dest_ipadr, dest_sock;
	char *fileName;
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
	senderBuffer = (char *)malloc(bufferSize * sizeof(char));
	windowBuffer = (Segment **)malloc(windowSize * sizeof(Segment *));
	statusTable = (char *)malloc(windowSize * sizeof(char));
	timeoutTable = (char *)malloc(windowSize * sizeof(char));

	// Init runtime vars
	lfs = 0;
	lar = 0;
	seqnum = 0;
	status = 0;

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
	if (bind(my_sock, (struct sockaddr *)&socket_sender, sizeof(socket_sender)) == -1) {
		int enable = 1;
		if (setsockopt(my_sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
			printf("Socket error: %d, %s\n", errno, strerror(errno));
			handleError("Error: Failed to bind socket\n");
		}
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

void handleError(const char *msg) {
	fprintf(stderr, "%s", msg);
	exit(1);
}

off_t fsize(const char *fileName) {
	struct stat st;

	if (stat(fileName, &st) == 0) {
		return st.st_size;
	} else {
		return -1;
	}
}

void *sendFile() {
	int sent_len;
	Segment *sentSegment;
	char finish = 0;

	while (status != 3) {
		if (!feof(fptr)) {
			// Read file
			fread(senderBuffer, bufferSize, 1, fptr);

			// Initialize status table
			for (int i = 0; i < windowSize; ++i) {
				statusTable[i] = 1;
				timeoutTable[i] = TIMEOUT;
			}
		}

		windowBufferPtr = 0;
		int i = 0;
		while (!finish) {
			// Convert data to segment
			int currWindowSize = lfs - lar + (status < 2);
			char advance = (currWindowSize <= windowSize) && (i < bufferSize) && (seqnum <= fileSize);
			if (advance) {
				sentSegment = (Segment *)malloc(sizeof(Segment));
				initSegment(sentSegment, seqnum, senderBuffer[i]);
				seqnum++;
				i++;
				if (status == 0) {
					status = 1;
				} else {
					lfs++;
				}

				// Fill sender buffer
				windowBuffer[windowBufferPtr] = sentSegment;
				statusTable[windowBufferPtr] = -1;
			}
			printf("LFS: %d, LAR: %d\n", lfs, lar);

			// Send data
			for (int tempLar = 0; tempLar < windowSize; ++tempLar) {
				if (statusTable[tempLar] == -1) {
					sentSegment = windowBuffer[tempLar];
					sent_len = sendto(my_sock, sentSegment, sizeof(Segment), 0, (struct sockaddr *)&socket_destination, sizeof(socket_destination));
					if (sent_len == -1) {
						handleError("Error: Failed to send data\n");
					}
					printf("Data %d sent\n", sentSegment->seqnum);
					statusTable[tempLar] = 0;
					timeoutTable[tempLar] = TIMEOUT;
				}
			}

			// Advance if smaller than window size and there's still data to send
			currWindowSize = lfs - lar + (status < 2);
			advance = (currWindowSize <= windowSize) && (i < bufferSize) && (seqnum <= fileSize);
			printf("currWindowSize = %d, windowSize = %d, i = %d, seqnum = %d\n", currWindowSize, windowSize, i, seqnum);
			if (advance) {
				windowBufferPtr = (windowBufferPtr + 1) % windowSize;
			}

			// Finish condition: all data sent successfully
			finish = 1;
			for (int tempLar = 0; tempLar < windowSize; ++tempLar) {
				finish &= (statusTable[tempLar] == 1);
			}
			finish &= ((lar == fileSize) || (lar % bufferSize == bufferSize - 1));
			printf("filesize: %jd; finish: %d\n", fileSize, finish);
			usleep(100000);
		}
	}

	pthread_exit(NULL);
}

void *receiveAck() {
	int recv_len;
	ACK *ack;
	ack = (ACK *)malloc(sizeof(ACK));
	while (status != 3) {
		// Receive ACK
		recv_len = recvfrom(my_sock, ack, sizeof(ACK), 0, NULL, NULL);
		if (recv_len != -1) {
			printf("Received ACK %d\n", ack->nextseq - 1);

			// Check if ACK is in order
			statusTable[(ack->nextseq - 1) % windowSize] = 1;
			if (ack->nextseq - 1 == lar + 1) {
				// Received ACK = LAR + 1
				printf("Received ACK in order\n");
				int tempLar = lar % windowSize;
				for (int i = (tempLar + 1) % windowSize; i != tempLar; i = (i + 1) % windowSize) {
					if (windowBuffer[i] != NULL) {
						if (statusTable[i] == 1 && lar < lfs && windowBuffer[i]->seqnum > tempLar) {
							lar++;
						} else {
							break;
						}
					}
				}
			}
		}

		if (status == 2 && lar >= fileSize - 1) {
			status = 3;
		}
	}

	pthread_exit(NULL);
}

void *timeout() {
	while (status != 3) {
		sleep(1);
		printf("Status table:\t| ");
		for (int i = 0; i < windowSize; ++i) {
			if (timeoutTable[i] <= 0) {
				if (statusTable[i] == 0)
					statusTable[i] = -1;
			} else {
				timeoutTable[i]--;
			}

			// Debug
			int seqnum = -1;
			if (windowBuffer[i] != NULL) {
				seqnum = windowBuffer[i]->seqnum;
			}
			printf("%d: %d\t| ", seqnum, statusTable[i]);
		}
		printf("\n");
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