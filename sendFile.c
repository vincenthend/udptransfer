#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include "frame.h"

#define PORT 8000

int main (int argc, char* argv[]){
	int my_port, my_ipadr, my_sock;
	int dest_port, dest_ipadr, dest_sock;
	struct sockaddr_in socket_sender, socket_destination;
	char msg[6];
	
	if(argc != 6){
		printf("Usage : ./sendfile <filename> <windowsize> <buffersize> <destination_ip> <destination_port>\n");
		exit(1);
	}
	else{
		////////////////////
		// Initialization //
		////////////////////
		
		// Set port used to send
		my_port = PORT;
		
		// Read arguments, Open file
		dest_ipadr = inet_addr(argv[4]);
		sscanf(argv[5], "%d", &dest_port);

		// Error checking of arguments
		
		/////////////////////
		// Socket Creation //
		/////////////////////
		
		// Initialize socket
		my_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if(my_sock == -1){
			printf("Error : Failed to initialize socket\n");
			exit(1);
		}
		
		socket_sender.sin_family = AF_INET;
		socket_sender.sin_port = htons(my_port);
		socket_sender.sin_addr.s_addr = htonl(INADDR_ANY);

		// Binding socket address to port
		if(bind(my_sock, (struct sockaddr*)&socket_sender, sizeof(socket_sender)) == -1){
			printf("Error : Failed to bind socket\n");
			exit(1);
		}
		
		///////////////////////
		//  Segment Sending  //
		///////////////////////

		// Setup Sending Address
		socket_destination.sin_family = AF_INET;
		socket_destination.sin_port = htons(dest_port);
		socket_destination.sin_addr.s_addr = dest_ipadr;

		printf("Sending to %s\n	",inet_ntoa(socket_destination.sin_addr));
		
		// Send data
		while(1){
			// Convert data to segment
			
			// Fill Sender Buffer
			
			// Send data
			if(sendto(my_sock, msg, sizeof(msg), 0, (struct sockaddr*) &socket_destination, sizeof(socket_destination)) == -1){
				printf("Error : Failed to send data\n");
			}
			
			// Wait for ACK
			
			// Move buffer head
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