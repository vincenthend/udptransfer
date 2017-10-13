#ifndef _FRAME_H
#define _FRAME_H

typedef struct{
	uint8_t soh;
	uint32_t seqnum;
	uint8_t stx;
	char data;
	uint8_t etx;
	int8_t checksum;
} Frame;

typedef struct{
	uint8_t ack;
	uint32_t nextseq;
	uint8_t advwinsize;
	int8_t checksum;
} ACK;

void initFrame(Frame* f, uint32_t seqnum, char data);

void initACK(ACK* f, uint32_t nextseq, uint8_t advwinsize);

#endif