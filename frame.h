#ifndef _FRAME_H
#define _FRAME_H

typedef struct{
	uint8_t soh;
	uint32_t seqnum;
	uint8_t stx;
	uint8_t data;
	uint8_t etx;
	int8_t checksum;
} Frame;

void initFrame(Frame* f, uint32_t seqnum, uint8_t data);

#endif