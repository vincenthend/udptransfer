#ifndef _BUFFER_H
#define _BUFFER_H

#include <stdint.h>
#include <stdio.h>
#include "frame.h"

/**
 * Tipe data segment, berukuran 9 byte
 */
typedef struct{
    int size;
    int head;
    int tail;
    Segment* segment;
} Buffer;

void initBuffer(int size);

void addElmt(Segment segment);

int getEmpty(Buffer b);
//void flushBuffer();

#endif