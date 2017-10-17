#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include "frame.h"

/**
 * Displays error message to stderr, then exits program with exitcode 1
 *
 * @param msg Error message
 */
void handleError();

/**
 * Initializes buffer table
 */
void initBufferTable(char* bufferTable, int size, int * advWindowSize);

/**
 * Flushes buffer to file
 */
void flushBuffer(FILE* file, char* buffer, int size);

int main (int argc, char* argv[]){
	int windowSize, bufferSize, advWindowSize;
	int my_port, my_ipadr, my_sock;
	int recv_len;
	char* bufferTable;
	char* receiverBuffer;
	Segment* receivedSegment;
	ACK ack;
	struct sockaddr_in socket_my, socket_sender;
	FILE * file;

	//Runtime var
	uint32_t nextSeq, lfr, laf, seqnum;
	int i, slen;
	slen = sizeof(socket_sender);
	
	if(argc != 5){
		printf("Usage : ./recvfile <filename> <windowsize> <buffersize> <port>\n");
		exit(1);
	}
	else{
		////////////////////
		// Initialization //
		////////////////////
		
		// Read arguments
		sscanf(argv[4], "%d", &my_port); // Receiving port
		sscanf(argv[3], "%d", &bufferSize); // Buffer size
		sscanf(argv[2], "%d", &windowSize); // Window size
		
		file = fopen(argv[1], "w");
		
		receiverBuffer = (char*) malloc(bufferSize * sizeof(char));
		//bufferTable = (char*) malloc(bufferSize * sizeof(char));
		receivedSegment = (Segment*) malloc(sizeof(Segment));
		
		//initBufferTable(bufferTable, sizeof(*bufferTable), &advWindowSize);
		
		// Init variables
		lfr = 0;
		laf = lfr + windowSize;
		nextSeq = 0;
		advWindowSize = bufferSize;
		
		/////////////////////
		// Socket Creation //
		/////////////////////
		
		// Initialize socket
		my_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if(my_sock == -1){
			handleError();
		}
		
		socket_my.sin_family = AF_INET;
		socket_my.sin_port = htons(my_port);
		socket_my.sin_addr.s_addr = htonl(INADDR_ANY);

		// Binding socket address to port
		if (bind(my_sock, (struct sockaddr*)&socket_my, sizeof(socket_my)) == -1){
			int enable = 1;
			if (setsockopt(my_sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
				handleError();
			}
		}
		
		/////////////////////////
		//  Segment Receiving  //
		/////////////////////////
		
		printf("Receiving on port :%d\n",my_port);
		// Receive Data
		do{
			recv_len = recvfrom(my_sock, receivedSegment, 9, 0, (struct sockaddr *) &socket_sender, &slen);
			fflush(stdout);
			
			if (recv_len != -1) {
				//Process received data, check the checksum
				if(receivedSegment->checksum == countSegmentChecksum(*receivedSegment)){
					seqnum = receivedSegment->seqnum;
					
					printf("Received Seqnum: %d, Data: %x\n",seqnum, receivedSegment->data);
					printf("LFR :%d ; LAF :%d\n",lfr, laf);
					
					if(seqnum % bufferSize >= lfr && seqnum % bufferSize <= laf){
						//Check if the segment is the requested next, if it does put it in buffer
						printf("Data in frame\n");
						
						if(nextSeq == seqnum){
							printf("data seqnum correct\n");
							receiverBuffer[seqnum] = receivedSegment->data;
							//bufferTable[seqnum] = 0x1;
							advWindowSize -= 1;
							
							nextSeq = nextSeq + 1;
							if(receivedSegment->seqnum % bufferSize != 0){ //Padding
								lfr = lfr + 1;
							}
							laf = lfr + windowSize;
							laf = (laf >= bufferSize) ? bufferSize - 1 : laf;
						}
						
						printf("Sending ACK nextseq = %d advwinsize = %d to %s:%d\n", nextSeq, advWindowSize, inet_ntoa(socket_sender.sin_addr), ntohs(socket_sender.sin_port));
						initACK(&ack, nextSeq, advWindowSize);
						
						if(advWindowSize == 0){
							advWindowSize = bufferSize;
							lfr = 0;
							laf = lfr + windowSize;
							flushBuffer(file, receiverBuffer, bufferSize);
							//initBufferTable(bufferTable, bufferSize, &advWindowSize);
						}
						
						if(sendto(my_sock, &ack, sizeof(ack), 0, (struct sockaddr*) &socket_sender, sizeof(socket_sender)) == -1){
							printf("Error : Failed to send ACK for seqnum %d\n", nextSeq);
						}
					}
				}
				else{
					printf("Received Misc Data: %x\n ", *receivedSegment);
				}
			}
			else{
				handleError();
			}
		} while(seqnum != -1);
		initACK(&ack, -1, advWindowSize);
		printf("Sending ACK nextseq = %d to %s:%d\n", -1, inet_ntoa(socket_sender.sin_addr), ntohs(socket_sender.sin_port));
		if(sendto(my_sock, &ack, sizeof(ack), 0, (struct sockaddr*) &socket_sender, sizeof(socket_sender)) == -1){
			printf("Error : Failed to send ACK for seqnum %d\n", nextSeq);
		}
		flushBuffer(file, receiverBuffer, bufferSize-advWindowSize);
		fclose(file);
	}
	return 0;
}

void handleError(){
	printf("Error : %s\n", strerror(errno));
	exit(1);
}

void initBufferTable(char* bufferTable, int size, int * advWindowSize){
	int i;
	for(i = 0; i<size; i++){
		bufferTable[i] = 0x0;
	}
	*advWindowSize = size;
}

void flushBuffer(FILE* file, char* buffer, int size){
	fwrite(buffer, 1, size, file);
}