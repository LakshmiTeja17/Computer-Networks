#include "packet.h"
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PDR 10

FILE *log_file;

int receive_data(int sock, packet *pkt) {
  struct sockaddr_in dest;
  int size = sizeof(struct sockaddr_in);
  int temp =
      recvfrom(sock, pkt, sizeof(*pkt), 0, (struct sockaddr *)&dest, &size);

  dest = (struct sockaddr_in)dest;
  int port = ntohs(dest.sin_port);
  return port;
}

void run_socket(int sock, struct sockaddr_in serverAddr,
                struct sockaddr_in clientAddr, char *relay_num) {

  packet pkt;

  do {
    int port = receive_data(sock, &pkt);
    int bytesSent;
    struct sockaddr_in dest;

    // Destination of packet decided based on whether the source is client or
    // server
    if (port == CLIENT_PORT) {
      printf("RELAY%s\tR\t%s\tDATA\t%d\tCLIENT\tRELAY%s\n", relay_num,
             get_timestamp(), pkt.seq_no, relay_num);

      fprintf(log_file, "RELAY%s\tR\t%s\tDATA\t%d\tCLIENT\tRELAY%s\n",
              relay_num, get_timestamp(), pkt.seq_no, relay_num);
      int num = rand() % 101;

      // Simulates the packet drop
      if (num < PDR) {
        printf("RELAY%s\tD\t%s\tDATA\t%d\t----\t----\n", relay_num,
               get_timestamp(), pkt.seq_no);
        fprintf(log_file, "RELAY%s\tD\t%s\tDATA\t%d\t----\t----\n", relay_num,
                get_timestamp(), pkt.seq_no);
        continue;
      }

      dest = serverAddr;
      printf("RELAY%s\tS\t%s\tDATA\t%d\tRELAY%s\tSERVER\n", relay_num,
             get_timestamp(), pkt.seq_no, relay_num);
      fprintf(log_file, "RELAY%s\tS\t%s\tDATA\t%d\tRELAY%s\tSERVER\n",
              relay_num, get_timestamp(), pkt.seq_no, relay_num);

      // Simulates the packet delay
      int delay = rand() % 3;
      usleep(delay * 1000);
    }

    if (port == SERVER_PORT) {
      printf("RELAY%s\tR\t%s\tACK\t%d\tSERVER\tRELAY%s\n", relay_num,
             get_timestamp(), pkt.seq_no, relay_num);
      fprintf(log_file, "RELAY%s\tR\t%s\tACK\t%d\tSERVER\tRELAY%s\n", relay_num,
              get_timestamp(), pkt.seq_no, relay_num);
      dest = clientAddr;
      printf("RELAY%s\tS\t%s\tACK\t%d\tRELAY%s\tCLIENT\n", relay_num,
             get_timestamp(), pkt.seq_no, relay_num);
      fprintf(log_file, "RELAY%s\tS\t%s\tACK\t%d\tRELAY%s\tCLIENT\n", relay_num,
              get_timestamp(), pkt.seq_no, relay_num);
    }

    bytesSent = sendto(sock, &pkt, sizeof(pkt), 0, (struct sockaddr *)&dest,
                       sizeof(dest));

    if (bytesSent != sizeof(pkt)) {
      printf("Error while sending message to client");
      exit(0);
    }

  } while (true);
}

int main(int argc, char *argv[]) {
  srand(time(NULL));

  log_file = fopen(LOG_FILE, "a");

  struct sockaddr_in serverAddr, clientAddr;

  int sock = create_socket();

  struct sockaddr_in RelayAddr;
  memset(&RelayAddr, 0, sizeof(RelayAddr));
  RelayAddr.sin_family = AF_INET;

  // The port assigned based on whether the current process is of relay1 or
  // relay2 which is decided by a command line argument
  if (strcmp(argv[1], "0") == 0) {
    RelayAddr.sin_port = htons(RELAY_PORT0);
  }
  if (strcmp(argv[1], "1") == 0) {
    RelayAddr.sin_port = htons(RELAY_PORT1);
  }

  RelayAddr.sin_addr.s_addr = htonl(INADDR_ANY);

  int bnd = bind(sock, (struct sockaddr *)&RelayAddr, sizeof(RelayAddr));
  if (bnd < 0) {
    printf("Error while binding\n");
    exit(0);
  }

  // Creation of Server address
  memset((char *)&serverAddr, 0, sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(SERVER_PORT);
  serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);

  // Creation of Client address
  memset((char *)&clientAddr, 0, sizeof(clientAddr));
  clientAddr.sin_family = AF_INET;
  clientAddr.sin_port = htons(CLIENT_PORT);
  clientAddr.sin_addr.s_addr = inet_addr(CLIENT_IP);

  // This is an infinite loop, as a relay node has no way of knowing when
  // communication between client and server stops in an UDP connection
  run_socket(sock, serverAddr, clientAddr, argv[1]);

  close(sock);
  fclose(log_file);
  exit(0);
}