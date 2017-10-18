#define _BSD_SOURCE
#define _GNU_SOURCE

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
#define TIMEOUT 100 // milliseconds
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
char status, eofReceived;

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
	// sscanf(argv[2], "%d", &windowSize);
	sscanf("12", "%d", &windowSize);
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
	eofReceived = 0;

	// Set buffer
	senderBuffer = (char*) malloc(bufferSize * sizeof(char));
	windowBuffer = (Segment**) malloc(windowSize * sizeof(Segment*));
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

	// printf("Sending to %s\n	", inet_ntoa(socket_destination.sin_addr));

	// Send data
	pthread_create(&tidSend, NULL, sendFile, NULL);
	pthread_create(&tidReceiveAck, NULL, receiveAck, NULL);
	pthread_create(&tidTimeout, NULL, timeout, NULL);

	pthread_join(tidSend, NULL);
	// printf("Sender thread terminated\n");
	pthread_join(tidReceiveAck, NULL);
	// printf("Receiver thread terminated\n");
	pthread_join(tidTimeout, NULL);
	// printf("Timeout thread terminated\n");

	// Closing
	// printf("Closing files, freeing buffers\n");
	// free(senderBuffer);
	// free(statusTable);
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
		// senderBuffer = (char*) malloc(bufferSize * sizeof(char));
		dataPtr += bufferSize;

		// Read file
		fread(senderBuffer, bufferSize, 1, fptr);
		// printf("Flushed buffer\n\n");

	}

	return dataPtr;
}

void *sendFile() {
	// printf("Sender thread started\n");
	// pthread_setname_np(pthread_self(), "sendThread");

	int i = 0;
	int sent_len;
	// char finish = 0;
	char advanced = 1;
	off_t dataPtr = readFile(0);
	windowBufferPtr = 0;
	Segment* sentSegment = (Segment*) malloc(sizeof(Segment));

	while (!eofReceived) {
		// Convert data to segment
		// Segment *sentSegment;
		int currWindowSize = lfs - lar + (status < 2);
		if ((currWindowSize <= windowSize) && (i < bufferSize) && (seqnum < fileSize) && advanced) {
			// sentSegment = (Segment*) malloc(sizeof(Segment));

			// Fill segment
			char data = (i == -1) ? lastBuffer : senderBuffer[i];
			initSegment(sentSegment, seqnum, data);

			// Fill sender buffer
			// if (windowBuffer[windowBufferPtr] != NULL) {
			// 	free(windowBuffer[windowBufferPtr]);
			// }
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
		// finish = 1;
		// off_t limit = (fileSize < windowSize) ? fileSize : windowSize;
		// for (int j = 0; j < limit; ++j) {
		// 	finish &= (statusTable[j] == 1);
		// }
		// finish = (lar == fileSize - 1);
		// if (fileSize == 0) {
		// 	finish = 1;
		// 	status = 2;
		// }
		if (lar == fileSize - 1 || fileSize == 0) {
			status = 2;

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
				// printf("Data %d sent: %c\n", sentSegment->seqnum, sentSegment->data);
				// if (sentSegment->seqnum == -1) sleep(3);
				statusTable[j] = 0;
				timeoutTable[j] = TIMEOUT;
			}
		}

		// Advance if smaller than window size and there's still data to send
		currWindowSize = lfs - lar + (status < 2);
		if ((currWindowSize <= windowSize) && (i < bufferSize) && (seqnum != EOF_SEQNUM)) {
			windowBufferPtr = (windowBufferPtr + 1) % windowSize;
			advanced = 1;
		}

		// Reread file if buffer runs out
		if (i == bufferSize - 1) {
			dataPtr = readFile(dataPtr);
			i = -1;
		}

		// Finish condition: all data sent successfully
		// finish = eofReceived;

		// Debug
		// printf("Status table:\t| ");
		// for (int j = 0; j < windowSize; ++j) {
		// 	int seqnum = -1;
		// 	if (windowBuffer[j] != NULL) {
		// 		seqnum = windowBuffer[j]->seqnum;
		// 	}
		// 	printf("%d: %d\t| ", seqnum, statusTable[j]);
		// }
		// printf("\n");
		usleep(100);
	}

	// pthread_join(tidReceiveAck, NULL);
	// printf("Receiver thread terminated\n");

	// Free segments
	// for (int i = 0; i < windowSize; ++i) {
	// 	if (windowBuffer[i] != NULL)
	// 		free(windowBuffer[i]);
	// }
	// free(windowBuffer);


	// pthread_join(tidTimeout, NULL);
	// printf("Timeout thread terminated\n");
	// printf("Terminating sender thread\n");
	exit(0);
	pthread_exit(NULL);
}

void *receiveAck() {
	// printf("Receiver thread started\n");
	// pthread_setname_np(pthread_self(), "receiverThread");

	int recv_len;
	ACK *ack;
	ack = (ACK*) malloc(sizeof(ACK));
	while (status != 3) {
		// Receive ACK
		recv_len = recvfrom(my_sock, ack, sizeof(ACK), 0, NULL, NULL);
		if (recv_len != -1) {
			if (countACKChecksum(*ack) == ack->checksum) {
				// printf("Received ACK %d\n", ack->nextseq - 1);
				if (status < 2) {
					status = 2;
					statusTable[0] = 1;
				}

				// Process ACK
				if (ack->nextseq > lar) {
					int nextLar = lar + 1;
					if (ack->nextseq != EOF_SEQNUM) {
						for (int i = nextLar; i < ack->nextseq; ++i) {
							statusTable[i % windowSize] = 1;
							lar++;
						}
					}
				}

				// Process EOF ACK
				if (ack->nextseq == EOF_SEQNUM && (lar == fileSize - 1 || fileSize == 0)) {
					// printf("EOF ACK\n");
					exit(0);
					int nextLar = lar + 1;
					for (int i = nextLar; i <= fileSize; ++i) {
						statusTable[i % windowSize] = 1;
						lar++;
					}
					status = 3;
					eofReceived = 1;
				}

				// Check if ACK is in order
				// printf("ack->nextseq = %d, lar = %d lfs = %d, filesize = %jd => %d\n", ack->nextseq, lar, lfs, fileSize, (ack->nextseq == EOF_SEQNUM && lar == fileSize - 1));
				// if ((ack->nextseq - 1 == lar + 1) || (ack->nextseq == EOF_SEQNUM && (lar == fileSize - 1 || fileSize == 0))) {
				// 	int nextLar = lar + 1;
				// 	if (ack->nextseq != EOF_SEQNUM) {
				// 		// Received ACK = LAR + 1
				// 		for (int i = nextLar; i < ack->nextseq; ++i) {
				// 			statusTable[i % windowSize] = 1;
				// 			lar++;
				// 		}
				// 	} else {
				// 		// Special case: received ACK == EOF
				// 		printf("EOF ACK\n");
				// 		for (int i = nextLar; i < fileSize; ++i) {
				// 			statusTable[i % windowSize] = 1;
				// 			lar++;
				// 		}
				// 		statusTable[fileSize % windowSize] = 1;
				// 		lar = fileSize;
				// 		status = 3;
				// 		eofReceived = 1;
				// 	}

				// 	// if (lar < fileSize) {
				// 	// 	int tempLar = lar % windowSize;
				// 	// 	char advance = 1;
				// 	// 	for (int i = (tempLar + 1) % windowSize; i != tempLar && advance; i = (i + 1) % windowSize) {
				// 	// 		if (windowBuffer[i] != NULL) {
				// 	// 			advance = statusTable[i] == 1 && lar < lfs && windowBuffer[i]->seqnum > lar;
				// 	// 			if (advance) {
				// 	// 				lar++;
				// 	// 			}
				// 	// 		}
				// 	// 	}
				// 	// }
				// }

				// // Debug
				// printf("Status table:\t| ");
				// for (int j = 0; j < windowSize; ++j) {
				// 	int seqnum = -1;
				// 	if (windowBuffer[j] != NULL) {
				// 		seqnum = windowBuffer[j]->seqnum;
				// 	}
				// 	printf("%d: %d\t| ", seqnum, statusTable[j]);
				// }
				// printf("\n");

				// free(ack);
				// ack = (ACK*) malloc(sizeof(ACK));
			}
		}
	}

	// pthread_join(tidTimeout, NULL);
	// printf("Timeout thread terminated\n");

	// if (ack != NULL) {
	// 	free(ack);
	// }
	// printf("Terminating receiver thread\n");
	exit(0);
	pthread_exit(NULL);
}

void *timeout() {
	// printf("Timeout thread started\n");
	// pthread_setname_np(pthread_self(), "timeoutThread");

	while (status != 3) {
		usleep(100);
		for (int i = 0; i < windowSize; ++i) {
			if (timeoutTable[i] == 0) {
				if (statusTable[i] == 0)
					statusTable[i] = -1;
			} else {
				timeoutTable[i]--;
			}
			if (status == 3) {
				exit(0);
			}
		}
	}

	// printf("Terminating timeout thread\n");
	exit(0);
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