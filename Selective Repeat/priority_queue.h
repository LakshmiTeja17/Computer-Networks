#include "packet.h"
#include <limits.h>
#include <stdio.h>

#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

void heap_init(packet *heap, int *heap_size);
void heap_insert(packet *heap, int *heap_size, packet pkt);
void delete_pkt(packet *heap, int *heap_size, int seq_no);

#endif