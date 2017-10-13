#include <stdint.h>
#include "frame.h"

void initFrame(Frame* f, uint32_t seqnum, char data)
{
	f->soh = 0x1;
	f->seqnum = seqnum;
	f->stx = 0x2;
	f->data = data;
	f->etx = 0x3
	
	//Hitung Checksum
	f->checksum = 0; // ini nanti diganti ya
}

void initACK(ACK* a, uint32_t nextseq, uint8_t advwinsize){
	a->ack = 0x6;
	a->nextseq = nextseq;
	a->advwinsize = advwinsize;
	
	//Hitung Checksum
	a->checksum = 0; // ini nanti diganti ya
}