/*
    Simple udp server
*/
#include<stdio.h> //printf
#include<string.h> //memset
#include<stdlib.h> //exit(0);
#include<arpa/inet.h>
#include<sys/socket.h>
#include "frame.h"
 
#define BUFLEN 512  //Max length of buffer
#define PORT 8000   //The port on which to listen for incoming data
 
void die(char *s)
{
    perror(s);
    exit(1);
}
 
int main(void)
{
    struct sockaddr_in si_me, si_other;
     
    int s, i, slen = sizeof(si_other) , recv_len;
    ACK* ack;
	Segment* segment;
	
	segment = (Segment*) malloc(sizeof(Segment));
	ack = (ACK*) malloc(sizeof(ACK));
     
    //create a UDP socket
    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        die("socket");
    }
     
    // zero out the structure
    memset((char *) &si_me, 0, sizeof(si_me));
     
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
	
	int port;
	printf("Port to send : ");	
	scanf("%d", &port);
	
	char ip[16];
	printf("IP to send to : ");
	scanf("%s", ip);
	
	si_other.sin_family = AF_INET;
    si_other.sin_port = htons(port);
    si_other.sin_addr.s_addr = inet_addr(ip);
     
    //bind socket to port
    if( bind(s , (struct sockaddr*)&si_me, sizeof(si_me) ) == -1)
    {
        die("bind");
    }
     
    //keep listening for data
	
	uint32_t seqnum = 0;
	char data;
	
    while(seqnum != -1)
    {
        printf("Input data (seqnum data):");
		scanf("%u %c",&seqnum, &data);
		
        fflush(stdout);
        
		initSegment(segment, seqnum, data);
		
		if (sendto(s, segment, sizeof(Segment), 0, (struct sockaddr*) &si_other, slen) == -1)
        {
            die("sendto()");
        }
		
        if ((recv_len = recvfrom(s, ack, sizeof(ACK), 0, (struct sockaddr *) &si_other, &slen)) == -1)
        {
            die("recvfrom()");
        }
         
        printf("Received ACK for seqnum %d advwinsize %d\n", ack->nextseq, ack->advwinsize);
    }
	
	shutdown(s, 2);
    return 0;
}