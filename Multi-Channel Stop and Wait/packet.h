#include<stdio.h>
#include<stdbool.h>

#ifndef PACKET_H
#define PACKET_H

#define PACKETSIZE 100
#define SERVER_PORT 12351
#define SERVER_IP "192.168.1.2"


typedef struct packet {
  char msg[PACKETSIZE];
  int size;
  int seq_no;
  bool is_last;
  bool is_data;
  int channel_id;
} packet;

int create_socket();

#endif
