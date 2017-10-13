#include <stdint.h>
#include "frame.h"

void initFrame(Frame* f, uint32_t seqnum, uint8_t data)
{
	f->soh = 0x01;
	f->seqnum = seqnum;
	f->data = data;
	
	//Hitung Checksum
	f->checksum = 0;
}