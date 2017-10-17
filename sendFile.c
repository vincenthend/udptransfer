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
#define TIMEOUT 3000 // milliseconds
#define EOF_SEQNUM -1


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
 * @return file size
 */
off_t fsize(const char *fileName);

/**
 * Opens next batch of file and increments dataPtr accordingly
 *
 * @param dataPtr How many bytes of data have been read
 * @return dataPtr incremented by bufferSize
 */
off_t readFile(off_t dataPtr);

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
uint32_t *timeoutTable;
Segment **windowBuffer;
char lastBuffer;
char status;

// Runtime variables
int windowBufferPtr;
uint32_t lfs, lar;
uint32_t seqnum;

pthread_t tidSend, tidReceiveAck, tidTimeout;

int main(int argc, char *argv[]) {
	int dest_port, dest_ipadr, dest_sock;
	char *fileName;

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

	// Init runtime vars
	lfs = 0;
	lar = 0;
	seqnum = 0;
	status = 0;

	// Set buffer
	windowBuffer = (Segment**) malloc(windowSize * sizeof(Segment *));
	statusTable = (char*) malloc(windowSize * sizeof(char));
	timeoutTable = (uint32_t*) malloc(windowSize * sizeof(char));

	// Initialize status table
	for (int i = 0; i < windowSize; ++i) {
		windowBuffer[i] = NULL;
		statusTable[i] = -2;
		timeoutTable[i] = TIMEOUT;
	}

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

	// Closing
	printf("Closing files, freeing buffers\n");
	free(senderBuffer);
	free(statusTable);
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

off_t readFile(off_t dataPtr) {
	if (dataPtr < fileSize) {
		// Backup last buffer char
		if (senderBuffer != NULL) {
			lastBuffer = senderBuffer[bufferSize - 1];
		}

		// Set buffer
		senderBuffer = (char*) malloc(bufferSize * sizeof(char));
		dataPtr += bufferSize;

		// Read file
		fread(senderBuffer, bufferSize, 1, fptr);
		printf("Flushed buffer\n\n");

	}

	return dataPtr;
}

void *sendFile() {
	printf("Sender thread started\n");

	int i = 0;
	int sent_len;
	Segment *sentSegment;
	char finish = 0;
	char advanced = 1;
	off_t dataPtr = readFile(0);
	windowBufferPtr = 0;

	while (!finish) {
		// Convert data to segment
		int currWindowSize = lfs - lar + (status < 2);
		if ((currWindowSize < windowSize) && (i < bufferSize) && (seqnum <= fileSize) && advanced) {
			sentSegment = (Segment*) malloc(sizeof(Segment));

			// Fill segment
			char data = (i == -1) ? lastBuffer : senderBuffer[i];
			initSegment(sentSegment, seqnum, data);

			// Fill sender buffer
			windowBuffer[windowBufferPtr] = sentSegment;
			statusTable[windowBufferPtr] = -1;

			// Advance pointers
			seqnum++;
			i++;
			if (status == 0) {
				status = 1;
			} else {
				lfs++;
			}
			advanced = 0;
		}

		// Send EOF if no more data to send
		finish = 1;
		for (int j = 0; j < windowSize; ++j) {
			finish &= (statusTable[j] == 1);
		}
		if (seqnum == fileSize + 1 && finish) {
			// Create segment
			sentSegment = (Segment*) malloc(sizeof(Segment));
			initSegment(sentSegment, EOF_SEQNUM, 0x00);

			// Fill sender buffer
			windowBuffer[windowBufferPtr] = sentSegment;
			statusTable[windowBufferPtr] = -1;
			seqnum = EOF_SEQNUM;
		}

		// Send data
		for (int j = 0; j < windowSize; ++j) {
			if (statusTable[j] == -1) {
				sentSegment = windowBuffer[j];
				sent_len = sendto(my_sock, sentSegment, sizeof(Segment), 0, (struct sockaddr *)&socket_destination, sizeof(socket_destination));
				if (sent_len == -1) {
					handleError("Error: Failed to send data\n");
				}
				printf("Data %d sent: %c\n", sentSegment->seqnum, sentSegment->data);
				statusTable[j] = 0;
				timeoutTable[j] = TIMEOUT;
			}
		}

		// Advance if smaller than window size and there's still data to send
		currWindowSize = lfs - lar + (status < 2);
		if ((currWindowSize < windowSize) && (i < bufferSize) && (seqnum != EOF_SEQNUM)) {
			windowBufferPtr = (windowBufferPtr + 1) % windowSize;
			advanced = 1;
		}

		// Reread file if buffer runs out
		if (i == bufferSize - 1) {
			dataPtr = readFile(dataPtr);
			i = -1;
		}

<<<<<<< HEAD
			// Finish condition: all data sent successfully
			finish = 1;
			for (int j = 0; j < windowSize; ++j) {
				finish &= (statusTable[j] == 1);
			}
			finish &= (lar == fileSize);
			usleep(20000);
=======
		// Finish condition: all data sent successfully
		finish = 1;
		for (int j = 0; j < windowSize; ++j) {
			finish &= (statusTable[j] == 1);
>>>>>>> 03a480b41ca050467f714054b658a09334ad3720
		}
		finish &= (lar == fileSize);
		usleep(1000);
	}

	// Free segments
	for (int i = 0; i < windowSize; ++i) {
		if (windowBuffer[i] != NULL)
			free(windowBuffer[i]);
	}
	free(windowBuffer);

	printf("Sender thread terminated\n");
	pthread_exit(NULL);
}

void *receiveAck() {
	printf("Receiver thread started\n");

	int recv_len;
	ACK *ack;
	ack = (ACK*) malloc(sizeof(ACK));
	while (status != 3) {
		// Receive ACK
		recv_len = recvfrom(my_sock, ack, sizeof(ACK), 0, NULL, NULL);
		if (recv_len != -1) {
			printf("Received ACK %d\n", ack->nextseq - 1);
			if (status < 2) {
				status = 2;
			}

			// Set status table
			int windowBufferAckIndex = (ack->nextseq - 1) % windowSize;
			if (ack->nextseq == EOF_SEQNUM) {
				windowBufferAckIndex = windowBufferPtr;
			}
			if (windowBuffer[windowBufferAckIndex]->seqnum == ack->nextseq - 1) {
				//statusTable[windowBufferAckIndex] = 1;
			}

			// Check if ACK is in order
			if ((ack->nextseq - 1 == lar + 1) || (ack->nextseq == EOF_SEQNUM && lar == fileSize - 1)) {
				// Received ACK = LAR + 1
				for (int i = lar + 1; i < ack->nextseq; ++i) {
					statusTable[i % windowSize] = 1;
				}

				// Special case: received ACK == EOF
				if (ack->nextseq == EOF_SEQNUM) {
					for (int i = lar + 1; i < fileSize; ++i) {
						statusTable[i % windowSize] = 1;
					}
					statusTable[fileSize] = 1;
					status = 3;
				}


				int tempLar = lar % windowSize;
				char advance = 1;
				for (int i = (tempLar + 1) % windowSize; i != tempLar && advance; i = (i + 1) % windowSize) {
					if (windowBuffer[i] != NULL) {
						advance = statusTable[i] == 1 && lar < lfs && windowBuffer[i]->seqnum > lar;
						if (advance) {
							lar++;
						}
					}
				}

				// Check if EOF is in sequence
				int i = (lar + 1) % windowSize;
				if (windowBuffer[i] != NULL) {
					if (statusTable[i] == 1 && windowBuffer[i]->seqnum == EOF_SEQNUM) {
						lar++;
					}
				}
			}

			// Debug
			printf("Status table:\t| ");
			for (int j = 0; j < windowSize; ++j) {
				int seqnum = -1;
				if (windowBuffer[j] != NULL) {
					seqnum = windowBuffer[j]->seqnum;
				}
				printf("%d: %d\t| ", seqnum, statusTable[j]);
			}
			printf("\n");
		}
	}

	printf("Receiver thread terminated\n");
	pthread_exit(NULL);
}

void *timeout() {
	printf("Timeout thread started\n");

	while (status != 3) {
		usleep(1000);
		for (int i = 0; i < windowSize; ++i) {
			if (timeoutTable[i] == 0) {
				if (statusTable[i] == 0)
					statusTable[i] = -1;
			} else {
				timeoutTable[i]--;
			}
		}
	}

	printf("Timeout thread terminated\n");
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