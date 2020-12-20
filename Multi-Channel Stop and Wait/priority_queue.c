#include "packet.h"
#include <limits.h>
#include <stdio.h>

void heap_init(packet *heap, int *heap_size) {
  *heap_size = 0;
  heap[0].seq_no = -INT_MAX;
}

void heap_insert(packet *heap, int *heap_size, packet pkt) {
  *heap_size = *heap_size + 1;
  heap[*heap_size] = pkt;

  int ind = *heap_size;
  while (heap[ind / 2].seq_no > pkt.seq_no) {
    heap[ind] = heap[ind / 2];
    ind /= 2;
  }
  heap[ind] = pkt;
}

void delete_pkt(packet *heap, int *heap_size, int seq_no) {

  packet pkt, last;
  int child;
  int ind;

  for (ind = 1; ind <= *heap_size; ind++) {
    if (heap[ind].seq_no == seq_no) {
      pkt = heap[ind];
      break;
    }
  }
  if (ind > *heap_size) {
    return;
  }

  last = heap[*heap_size];
  *heap_size = *heap_size - 1;

  for (; ind * 2 <= *heap_size; ind = child) {

    child = ind * 2;

    if (child != *heap_size && heap[child + 1].seq_no < heap[child].seq_no) {
      child++;
    }

    if (last.seq_no > heap[child].seq_no) {
      heap[ind] = heap[child];
    } else {
      break;
    }
  }

  heap[ind] = last;
}