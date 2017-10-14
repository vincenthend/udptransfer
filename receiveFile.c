#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include "frame.h"

#define PORT 8000

int main (int argc, char* argv[]){
	int windowsize;
	int my_port, my_ipadr, my_sock;
	int recv_len;
	Segment* receivedSegment;
	Segment* receiverBuffer;
	ACK ack;
	struct sockaddr_in socket_my, socket_sender;

	uint32_t nextSeq, lfr, laf;
	
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
		sscanf(argv[3], "%d", &windowsize); // Window size
		receiverBuffer = (Segment*) malloc(windowsize * sizeof(Segment));

		// Init variables
		lfr = -1;
		laf = windowSize;
		nextSeq = 0;
		
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
		if(bind(my_sock, (struct sockaddr*)&socket_my, sizeof(socket_my)) == -1){
			printf("Error : Failed to bind socket\n");
			exit(1);
		}
		
		/////////////////////////
		//  Segment Receiving  //
		/////////////////////////
		
		printf("Receiving on port :%d\n",my_port);

		// Receive Data
		while(1){
			recv_len = recvfrom(my_sock, (char*) receivedSegment, 9, 0, (struct sockaddr *) &socket_sender, sizeof(socket_sender));
			if (recv_len != -1){
				//Process received data, check the checksum
				if(receivedSegment->checksum == countSegmentChecksum(*receivedSegment)){
					//Check if the segment is the requested next, if it does put it in buffer
					if(nextSeq == receivedSegment->seqnum){
						receiverBuffer[receivedSegment->seqnum - nextSeq] = *receivedSegment;
						nextSeq = nextSeq + 1;
					}
					
					initACK(&ack, nextSeq, );
					sendto(my_sock, ack, sizeof(ack), 0, (struct sockaddr*) &socket_sender, sizeof(socket_sender)) == -1);
				}
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