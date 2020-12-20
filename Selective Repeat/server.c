#include "packet.h"
#include "priority_queue.h"
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CHAR_BUFFER_SIZE 200

FILE *out_file;
FILE *log_file;
int last_seq_no = -2; // The maximum sequence number of a packet
int buff_seq_no =
    -1; // The cumulative sequence number written to the temporary
        // character buffer yet. It contains the in-order data only

packet pkt_buffer[WINDOWSIZE]; // Sliding window implemented as a priority queue
char ch_buffer[CHAR_BUFFER_SIZE]; // The temporary character buffer where data
                                  // is stored before being written to the
                                  // output file
int pkt_buffer_size, ch_buffer_size = 0;

int receive_client_data(int sock, packet *pkt) {
  struct sockaddr_in dest;
  int size = sizeof(struct sockaddr_in);
  int temp =
      recvfrom(sock, pkt, sizeof(*pkt), 0, (struct sockaddr *)&dest, &size);
  return temp;
}

// Makes acknowledgement packet of a given packet pkt
void make_ack_packet(packet *pkt_ack, packet pkt) {
  pkt_ack->seq_no = pkt.seq_no;
  pkt_ack->is_last = pkt.is_last;
  pkt_ack->channel_id = pkt.channel_id;
  pkt_ack->is_data = false;
}

void run_socket(int sock, struct sockaddr_in dest0, struct sockaddr_in dest1) {

  packet pkt, pkt_ack;

  do {

    // If priority queue is filled up
    if (pkt_buffer_size >= WINDOWSIZE - 1) {
      continue;
    }

    int rcv = receive_client_data(sock, &pkt);

    // No data recieved
    if (rcv < 1) {
      printf("problem in recieving data");
      break;
    }

    if (pkt.channel_id == 0) {
      printf("SERVER\tR\t%s\tDATA\t%d\tRELAY1\tSERVER\n", get_timestamp(),
             pkt.seq_no);
      fprintf(log_file, "SERVER\tR\t%s\tDATA\t%d\tRELAY1\tSERVER\n",
              get_timestamp(), pkt.seq_no);
    } else {
      printf("SERVER\tR\t%s\tDATA\t%d\tRELAY2\tSERVER\n", get_timestamp(),
             pkt.seq_no);
      fprintf(log_file, "SERVER\tR\t%s\tDATA\t%d\tRELAY2\tSERVER\n",
              get_timestamp(), pkt.seq_no);
    }

    // If priority queue has only one space empty and if packet recieved is not
    // the packet with sequence number just after the cumulative sequence number
    // written to the character buffer.
    // This condition necessary to prevent priority queue overflow
    if (pkt_buffer_size == WINDOWSIZE - 2 && pkt.seq_no != (buff_seq_no + 1)) {
      continue;
    }

    if (pkt.is_last == true) {
      last_seq_no = pkt.seq_no;
    }

    int bytesSent;
    pkt_ack.is_data = false;

    heap_insert(pkt_buffer, &pkt_buffer_size, pkt);

    // Writes those packets from priority queue to character buffer which have
    // sequence numbers just after the cumulative seqence number already written
    // to character buffer (So that out of order packets can be dealt with). If
    // character buffer is completely filled up, it's contents written to the
    // output file
    while (pkt_buffer_size > 0) {
      if (pkt_buffer[1].seq_no == (buff_seq_no + 1)) {
        if ((strlen(pkt_buffer[1].msg) + ch_buffer_size) >= CHAR_BUFFER_SIZE) {
          fprintf(out_file, "%s", ch_buffer);
          ch_buffer[0] = '\0';
          ch_buffer_size = 0;
        }

        strcat(ch_buffer, pkt_buffer[1].msg);
        ch_buffer_size += strlen(pkt_buffer[1].msg);
        buff_seq_no++;

        // Packet deleted from priority queue after it has been written to
        // character buffer
        delete_pkt(pkt_buffer, &pkt_buffer_size, pkt_buffer[1].seq_no);
      }

      else {
        break;
      }
    }

    make_ack_packet(&pkt_ack, pkt);

    // The ACK is sent to either Relay1 or Relay2 randomly. To simulate the fact
    // that the ACK packet can go to the client by any path
    int randomBit = rand() % 2;
    struct sockaddr_in dest;
    if (randomBit == 0) {
      dest = dest0;
      pkt_ack.channel_id = 0;
      printf("SERVER\tS\t%s\tACK\t%d\tSERVER\tRELAY1\n", get_timestamp(),
             pkt.seq_no);
      fprintf(log_file, "SERVER\tS\t%s\tACK\t%d\tSERVER\tRELAY1\n",
              get_timestamp(), pkt.seq_no);
    } else {
      dest = dest1;
      pkt_ack.channel_id = 1;
      printf("SERVER\tS\t%s\tACK\t%d\tSERVER\tRELAY2\n", get_timestamp(),
             pkt.seq_no);
      fprintf(log_file, "SERVER\tS\t%s\tACK\t%d\tSERVER\tRELAY2\n",
              get_timestamp(), pkt.seq_no);
    }

    bytesSent = sendto(sock, &pkt_ack, sizeof(pkt_ack), 0,
                       (struct sockaddr *)&dest, sizeof(dest));

    if (bytesSent != sizeof(pkt_ack)) {
      printf("Error while sending message to client");
      exit(0);
    }
  } while (buff_seq_no != last_seq_no);
  // Exits when the packet with the last_seq_no is written to the character
  // buffer

  // Empties all the residual contents of character buffer to the output file
  fprintf(out_file, "%s", ch_buffer);
}

int main(int argc, char *argv[]) {

  if (CHAR_BUFFER_SIZE < PACKETSIZE) {
    printf("Server's character buffer size is smaller than packet size!! "
           "Either decrease PACKETSIZE or increase CHAR_BUFFER_SIZE\n");
  }

  srand(time(NULL));
  heap_init(pkt_buffer, &pkt_buffer_size);
  out_file = fopen(argv[1], "w");
  log_file = fopen(LOG_FILE, "a");

  struct sockaddr_in relayAddr0, relayAddr1;

  int serverSocket = create_socket();

  struct sockaddr_in serverAddress, clientAddress0, clientAddress1;

  memset(&serverAddress, 0, sizeof(serverAddress));
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(SERVER_PORT);
  serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

  int bnd = bind(serverSocket, (struct sockaddr *)&serverAddress,
                 sizeof(serverAddress));
  if (bnd < 0) {
    printf("Error while binding\n");
    exit(0);
  }

  // Creating Relay1 address
  memset((char *)&relayAddr0, 0, sizeof(relayAddr0));
  relayAddr0.sin_family = AF_INET;
  relayAddr0.sin_port = htons(RELAY_PORT0);
  relayAddr0.sin_addr.s_addr = inet_addr(RELAY_IP);

  // Creating Relay2 address
  memset((char *)&relayAddr1, 0, sizeof(relayAddr1));
  relayAddr1.sin_family = AF_INET;
  relayAddr1.sin_port = htons(RELAY_PORT1);
  relayAddr1.sin_addr.s_addr = inet_addr(RELAY_IP);

  run_socket(serverSocket, relayAddr0, relayAddr1);

  close(serverSocket);
  pthread_exit(NULL);
  fclose(log_file);
  fclose(out_file);
  exit(0);
}
