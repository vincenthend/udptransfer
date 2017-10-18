#ifndef _FRAME_H
#define _FRAME_H

#include <stdint.h>
#include <stdio.h>

/**
 * Tipe data segment, berukuran 9 byte
 */
typedef struct{
	uint8_t soh;
	uint32_t seqnum;
	uint8_t stx;
	char data;
	uint8_t etx;
	int8_t checksum;
} __attribute__((packed)) Segment;

/**
 * Tipe data ACK, berukuran 7 byte
 */
typedef struct{
	uint8_t ack;
	uint32_t nextseq;
	uint8_t advwinsize;
	int8_t checksum;
} __attribute__((packed)) ACK;

void initSegment(Segment* s, uint32_t seqnum, char data);

void initACK(ACK* a, uint32_t nextseq, uint8_t advwinsize);

int8_t countSegmentChecksum(Segment s);

int8_t countACKChecksum(ACK a);

#endif