#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "frame.h"

int main (int argc, char* argv[]){
	int windowSize, bufferSize, advWindowSize;
	int my_port, my_ipadr, my_sock;
	int recv_len;
	char* receiverBuffer;
	char buf[500];
	Segment* receivedSegment;
	ACK ack;
	struct sockaddr_in socket_my, socket_sender;

	//Runtime var
	uint32_t nextSeq, lfr, laf, seqnumtoack;
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

		receiverBuffer = (char*) malloc(bufferSize * sizeof(char));
		receivedSegment = (Segment*) malloc(sizeof(Segment));

		// Init variables
		lfr = 0;
		laf = lfr + windowSize;
		nextSeq = 0;
		seqnumtoack = 0;
		advWindowSize = windowSize;
		
		/////////////////////
		// Socket Creation //
		/////////////////////
		
		// Initialize socket
		my_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if(my_sock == -1){
			printf("Error : Failed to initialize socket\n");
			exit(1);
		}
		
		socket_my.sin_family = AF_INET;
		socket_my.sin_port = htons(my_port);
		socket_my.sin_addr.s_addr = htonl(INADDR_ANY);

		// Binding socket address to port
		if (bind(my_sock, (struct sockaddr*)&socket_my, sizeof(socket_my)) == -1){
			printf("Error : Failed to bind socket\n");
			exit(1);
		}
		
		/////////////////////////
		//  Segment Receiving  //
		/////////////////////////
		
		printf("Receiving on port :%d\n",my_port);
		// Receive Data
		while(1){
			recv_len = recvfrom(my_sock, buf, 9, 0, (struct sockaddr *) &socket_sender, &slen);
			fflush(stdout);
			if (recv_len != -1) {
				receivedSegment = (Segment*) buf;
				//Process received data, check the checksum
				if(receivedSegment->checksum == countSegmentChecksum(*receivedSegment)){
					printf("Received Seqnum: %d, Data: %c\n",receivedSegment->seqnum, receivedSegment->data);
					printf("LFR :%d ; LAF :%d\n",lfr, laf);
					if(receivedSegment->seqnum >= lfr && receivedSegment->seqnum <= laf){
						//Check if the segment is the requested next, if it does put it in buffer
						receiverBuffer[receivedSegment->seqnum] = receivedSegment->data;
						printf("Data in frame\n");
						if(nextSeq == receivedSegment->seqnum){
							nextSeq = receivedSegment->seqnum + 1;
							if(receivedSegment->seqnum != 0){ //Padding
								lfr = lfr + 1;
							}
							laf = lfr + windowSize;
						}
						if (seqnumtoack < receivedSegment->seqnum){
							seqnumtoack = receivedSegment->seqnum;
						}
						printf("Sending ACK nextseq = %d to %s on port %d\n", nextSeq, inet_ntoa(socket_sender.sin_addr), ntohs(socket_sender.sin_port));
						initACK(&ack, nextSeq, advWindowSize);
						if(sendto(my_sock, &ack, sizeof(ack), 0, (struct sockaddr*) &socket_sender, sizeof(socket_sender)) == -1){
							printf("Error : Failed to send ACK for seqnum %d\n", nextSeq);
						}
					}
				}
				else{
					printf("Received Misc Data : ");
					printf("%x ",*receivedSegment);
					printf("\n");
				}
			}
			else{
				printf("Error : %s\n", strerror(errno));
				exit(1);
			}
		}
	}
	return 0;
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