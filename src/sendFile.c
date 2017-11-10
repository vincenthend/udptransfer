#define _BSD_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include "frame.h"
#include "sendingWindow.h"

#define TIMEOUT 300000

char* buffer;

// File, buffer variables
FILE* file;
uint32_t bufferSize;
uint32_t bufferOffset;
uint32_t bufferCur;
char fileDone;

// Socket variables
const uint32_t socketSize = sizeof(struct sockaddr_in);
int udpSocket;
struct sockaddr_in destAddr;

// Window variables
SendingWindow window;
uint32_t windowSize;
uint32_t advWindowSize;

char done;

void die(const char* msg) {
    perror(msg);
	exit(1);
}

int main(int argc, char* argv[]) {
    if (argc != 6) {
		die("Usage: ./sendfile <filename> <windowsize> <buffersize> <destination_ip> <destination_port>\n");
    }

    ////////////////////
	// Initialization //
    ////////////////////
    
    // Scan arguments
    const char* fileName = argv[1];
	windowSize = 128;
	bufferSize = 4096;
	const char* destIP = argv[4];
    const int destPort = atoi(argv[5]);
    
    // Open file
    file = fopen(fileName, "r");
	if (file == NULL) {
		die("Unable to open file");
    }
    buffer = (char*) malloc(windowSize * sizeof(char));

    /////////////////////
	// Socket Creation //
    /////////////////////

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
    memset((char*) &destAddr, 0, socketSize);
	destAddr.sin_family = AF_INET;
	destAddr.sin_port = htons(destPort);
	destAddr.sin_addr.s_addr = inet_addr(destIP);

    ///////////////////////
	//  Segment Sending  //
    ///////////////////////
    
    // Segment setup
    bufferCur = 0;
    fileDone = 0;
    advWindowSize = windowSize;
    bufferOffset = 0;

    // Window setup
    init(&window, windowSize);

    //////////////////////
	//  Sliding Window  //
    //////////////////////
    done = 0;
    while (!done) {
        // printf("Sender: sending data\n");

        // Buffer file
        char currData;
        while (bufferCur < bufferSize && !fileDone) {
			fileDone = (fscanf(file, "%c", &currData) == EOF);
            if (!fileDone) {
                buffer[bufferCur++] = currData;
                // printf("Sender: read data %u: %c\n", bufferCur - 1, buffer[bufferCur - 1]);
            }
        }
        
        // Sending frame
        Segment packet;
        uint32_t nSent = 0;
        
        while (LFS(window) < LAR(window) + windowSize &&
                LFS(window) < bufferCur + bufferOffset &&
                nSent < advWindowSize) {
            
            // Create segment
            initSegment(&packet, LFS(window), buffer[LFS(window) - bufferOffset]);
            // printf("Sender: frame #%u: %c\n", LFS(window), packet.data);
            LFS(window) += 1;

            int32_t sentLen = sendto(udpSocket, (char*) &packet, sizeof(Segment), 0, (struct sockaddr*) &destAddr, socketSize);
            if (sentLen == -1) {
                die("Error: Failed to send data");
            }

            nSent++;
        }

        // Sending lost frame
        // printf("Sender: resending lost frame(s)\n");
        uint32_t repeatStart = LAR(window);
		uint32_t repeatEnd = LFS(window);
        for (uint32_t i = repeatStart; i < repeatEnd; ++i) {
            initSegment(&packet, i, buffer[i - bufferOffset]);
            
            // printf("Sender: sending frame #%u: %c\n", i, packet.data);
            int32_t sentLen = sendto(udpSocket, (char*) &packet, sizeof(Segment), 0, (struct sockaddr*) &destAddr, socketSize);
            if (sentLen == -1) {
                die("Error: Failed to send data");
            }
            nSent++;
        }

        ACK ack;
        for (uint32_t i = 0; i < nSent; ++i) {
            int32_t recvLen = recvfrom(udpSocket, (char*) &ack, sizeof(ACK), 0, NULL, NULL);
            if (recvLen != -1) {
                if (countACKChecksum(ack) == ack.checksum) {
                    // printf("Sender: ACK received, next sequence number = %u\n", ack.nextseq);
                    advWindowSize = ack.advwinsize;
                    LAR(window) = ack.nextseq;
                } else {
                    // printf("Sender: wrong checksum\n");
                }
            }
        }

        // Check stop condition
        done = (fileDone && LAR(window) >= bufferCur + bufferOffset);
        // printf("LAR: %u, bufferCur: %u, bufSizeOff: %u -> done: %d\n", LAR(window), bufferCur, bufferOffset, done);

        // Increase buffer size offset
        if (LAR(window) == bufferSize + bufferOffset) {
            bufferCur = 0;
            bufferOffset += bufferSize;
            // printf("Sender: increase buffer size offset to %u\n", bufferOffset);
        }
    }

    ///////////////////////
	//  Ending Sequence  //
    ///////////////////////
    Segment finalPacket;
    initSegment(&finalPacket, 0, 0);
    finalPacket.soh = 0x2;
    finalPacket.checksum = countSegmentChecksum(finalPacket);

    ACK finalACK;
    initACK(&finalACK, 0, 0);

    printf("Sender: sending EOF packet\n");
    while (finalACK.nextseq == 0 || finalACK.checksum != countACKChecksum(finalACK)) {
        sendto(udpSocket, (char*) &finalPacket, sizeof(Segment), 0, (struct sockaddr*) &destAddr, socketSize);
        recvfrom(udpSocket, (char*) &finalACK, sizeof(ACK), 0, NULL, NULL);
    }

    fclose(file);
    return 0;
}
