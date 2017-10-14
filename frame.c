#include "frame.h"

/**
 * Melakukan inisialisasi pada Segment dengan seqnum dan data
 */
void initSegment(Segment* s, uint32_t seqnum, char data)
{
	s->soh = 0x1;
	s->seqnum = seqnum;
	s->stx = 0x2;
	s->data = data;
	s->etx = 0x3;
	
	//Hitung Checksum
	s->checksum = countSegmentChecksum(*s);
}

/**
 * Melakukan inisialisasi pada ACK dengan nextseq dan advwinsize
 */
void initACK(ACK* a, uint32_t nextseq, uint8_t advwinsize){
	a->ack = 0x6;
	a->nextseq = nextseq;
	a->advwinsize = advwinsize;
	
	//Hitung Checksum
	a->checksum = countACKChecksum(*a);
}

/**
 * Menghitung nilai checksum pada Segment
 */
int8_t countSegmentChecksum(Segment s){
	int8_t checksum = 0;
	int8_t* data = (int8_t*)&s;
	int8_t i;
	
	for(i=0; i<8; i++){
		checksum += data[i];
	}

	return checksum;
}

/**
 * Menghitung nilai checksum pada ACK
 */
int8_t countACKChecksum(ACK a){
	int8_t checksum = 0;
	int8_t* data = (int8_t*)&a;
	int i;

	for(i=0; i<6; i++){
		checksum += data[i];
	}

	return checksum;
}