#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "frame.h"

#define TIMEOUT 300000

uint32_t bufferSize;

void die(const char* msg) {
    perror(msg);
	exit(1);
}

void flush(char* bufferTable) {
	int i;
	for (i = 0; i < bufferSize; i++) {
		bufferTable[i] = 0;
	}
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        die("Usage : ./recvfile <filename> <windowsize> <buffersize> <port>");
    }
    
    ////////////////////
    // Initialization //
    ////////////////////

    // Scan arguments
	const char* fileName = argv[1];
	const uint32_t windowSize = 8;
	bufferSize = 256;
    const int destPort = atoi(argv[4]);

    FILE* file = fopen(fileName, "w");
    if (file == NULL) {
		die("Error: Unable to open file");
    }
    char* buffer = (char*) malloc(bufferSize * sizeof(char));
    char* bufferTable = (char*) malloc(bufferSize * sizeof(char));

    /////////////////////
	// Socket Creation //
    /////////////////////

	int udpSocket;
	struct sockaddr_in srcAddr, destAddr;
	uint32_t socketSize = sizeof(struct sockaddr_in);

    // Socket creation
	if ((udpSocket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		die("Error: Failed to initialize socket");
    }

    // Timeout setting
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = TIMEOUT;
    setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, (char*) &timeout, sizeof(struct timeval));
    
    // Setup destination address
    destAddr.sin_family = AF_INET;
	destAddr.sin_port = htons(destPort);
    destAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind socket
    if (bind(udpSocket, (struct sockaddr*) &destAddr, socketSize) == -1) {
        int enable = 1;
        if (setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
            die(strerror(errno));
        }
    }

    /////////////////////////
	//  Segment Receiving  //
    /////////////////////////
    
    uint32_t lfr = 0;
    uint32_t laf = windowSize;
    uint32_t bufferCur = 0;
    uint32_t bufferOffset = 0;
    uint32_t advWindowSize = (windowSize < bufferSize) ? windowSize : bufferSize;
    char done = 0;

    flush(bufferTable);
    srand(time(NULL));

    while (!done) {
        // printf("Receiver: Waiting for data...");
        fflush(stdout);

        Segment packet;
        int32_t recvLen = recvfrom(udpSocket, (char*) &packet, sizeof(Segment), 0, (struct sockaddr*) &srcAddr, &socketSize);
        if (recvLen != -1) {
            if (packet.checksum == countSegmentChecksum(packet)) {
                done = (packet.soh == 0x02);

                // printf("Receiver: received packet #%u: %c\n", packet.seqnum, packet.data);
                if (packet.seqnum >= lfr && packet.seqnum <= laf) {
                    if (packet.seqnum == lfr) {
                        lfr++;

                        buffer[packet.seqnum - bufferOffset] = packet.data;
                        bufferCur++;
                        advWindowSize--;
                        if (advWindowSize == 0) {
                            advWindowSize = (windowSize < bufferSize) ? windowSize : bufferSize;
                        }
                    }
                }

                laf = lfr + ((windowSize < advWindowSize) ? windowSize : advWindowSize);
                // printf("Receiver: LFR: %u, LAF: %u\n", lfr, laf);
            } else {
                // printf("Receiver: wrong checksum for packet #%u: %d | %d\n", packet.seqnum, packet.checksum, countSegmentChecksum(packet));
            }
        }

        // Sending ACK
        ACK ack;
        initACK(&ack, lfr, advWindowSize);

        // printf("Receiver: Sending ACK #%u\n", ack.nextseq);
        int32_t sentLen = sendto(udpSocket, (char*) &ack, sizeof(ACK), 0, (struct sockaddr*) &srcAddr, socketSize);
        if (sentLen == -1) {
            die("Receiver: Error: Failed to send ACK");
        }

        // Write data to file
        if (bufferCur == bufferSize) {
            // printf("Receiver: Writing data to file\n");
            bufferOffset += bufferSize;
            bufferCur = 0;

            for (uint32_t i = 0; i < bufferSize; ++i) {
                fputc(buffer[i], file);
            }
        }
    }

    if (bufferCur != 0) {
        printf("Receiver: Writing remaining data to file\n");

        for (uint32_t i = 0; i < bufferCur; ++i) {
            fputc(buffer[i], file);
        }
    }
    printf("Receiver: All data received");
    
    fclose(file);
    return 0;
}