#include<stdio.h>
#include<stdbool.h>

#ifndef PACKET_H
#define PACKET_H

#define PACKETSIZE 100
#define WINDOWSIZE 10

#define RELAY_PORT0 8888
#define RELAY_PORT1 8889
#define SERVER_PORT 8900
#define CLIENT_PORT 8901

#define RELAY_IP "192.168.1.2"
#define CLIENT_IP "192.168.1.2"
#define SERVER_IP "192.168.1.2"

#define LOG_FILE "log.txt"

typedef struct packet {
  char msg[PACKETSIZE];
  int size;
  int seq_no;
  bool is_last;
  bool is_data;
  int channel_id;

} packet;

int create_socket();
char *get_timestamp();

#endif