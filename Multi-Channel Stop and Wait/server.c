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

#define MAXPENDING 5
#define PDR 10
#define BUFFERSIZE 50

FILE *out_file;
pthread_mutex_t lock;
int prev_seq_no; // The cumulative sequence number written to the file

packet
    buffer[BUFFERSIZE]; // The temporary buffer implemented as a priority queue
int buffer_size = 0;

int receive_client_data(int sock, packet *pkt) {
  int temp = recv(sock, pkt, sizeof(*pkt), 0);
  return temp;
}

// Makes acknowledgement packet of a given packet pkt
void make_ack_packet(packet *pkt_ack, packet pkt) {
  pkt_ack->seq_no = pkt.seq_no;
  pkt_ack->is_last = pkt.is_last;
  pkt_ack->is_data = false;
  pkt_ack->channel_id = pkt.channel_id;
}

void *run_socket(void *arg) {
  int *temp = (int *)arg;
  int sock = *temp;
  packet pkt, pkt_ack;
  bool is_dropped;

  do {

    int rcv = receive_client_data(sock, &pkt);

    // No packet recieved
    if (rcv < 1) {
      printf("problem in recieving data");
      break;
    }

    printf("RCVD PKT: Seq. No %d of size %d Bytes from channel %d\n",
           pkt.seq_no, pkt.size, pkt.channel_id);

    int bytesSent;

    // Simulates the packet drop.
    int num = (rand() % (101));

    // If packet is not dropped
    if (num > PDR) {
      is_dropped = false;
      // Mutex used to synchronise access to the buffer.
      pthread_mutex_lock(&lock);

      // If priority queue has only one space empty and if packet recieved is
      // not
      // the packet with sequence number just after the cumulative sequence
      // number written to the character buffer. This condition necessary to
      // prevent priority queue overflow by dropping a packet from end of the
      // priority queue
      if (buffer_size == BUFFERSIZE - 2 && pkt.seq_no != (prev_seq_no + 1)) {
        printf("Buffer overflow at the server!! Increase BUFFERSIZE in "
               "server.c\n");
        exit(0);
      }

      heap_insert(buffer, &buffer_size, pkt);

      // Writes those packets from buffer to output file which have sequence
      // numbers just after the cumulative seqence number already written to
      // file.
      while (buffer_size > 0) {
        if (buffer[1].seq_no == (prev_seq_no + PACKETSIZE - 1)) {
          fprintf(out_file, "%s", buffer[1].msg);
          prev_seq_no = prev_seq_no + PACKETSIZE - 1;

          // Packet deleted from priority queue after it has been written to
          // the output
          delete_pkt(buffer, &buffer_size, buffer[1].seq_no);
        }

        else {
          break;
        }
      }

      pthread_mutex_unlock(&lock);

      make_ack_packet(&pkt_ack, pkt);
      bytesSent = send(sock, &pkt_ack, sizeof(pkt_ack), 0);
      printf("SENT ACK: for PKT with Seq. No. %d from channel %d\n", pkt.seq_no,
             pkt.channel_id);
    }
    // The case where packets are dropped. bytesSent initialized as below so
    // that no error is thrown later.
    else {
      is_dropped = true;
      bytesSent = sizeof(pkt_ack);
    }

    if (bytesSent != sizeof(pkt_ack)) {
      printf("Error while sending message to client");
      exit(0);
    }
  } while (!(pkt.is_last == true && is_dropped == false));
  // Exits when the channel's last packet has been recieved and this last packet
  // hasn't been dropped. This works because, in a particular channel, stop and
  // wait protocol ensures that al packets are recieved in ascending order of
  // their sequence numbers. So, this channel won't recieve any other packets.
}

int main(int argc, char *argv[]) {

  srand(time(NULL));
  int serverSocket = create_socket();
  prev_seq_no =
      -(PACKETSIZE - 1); // Initialized so that next packet to be written to
                         // file will have sequence number 0.
  struct sockaddr_in serverAddress, clientAddress0, clientAddress1;
  memset(&serverAddress, 0, sizeof(serverAddress));
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(SERVER_PORT);
  serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

  int bnd = bind(serverSocket, (struct sockaddr *)&serverAddress,
                 sizeof(serverAddress));
  if (bnd < 0) {
    printf("Error while binding. Try a different SERVER_PORT\n");
    exit(0);
  }

  int temp1 = listen(serverSocket, MAXPENDING);
  if (temp1 < 0) {
    printf("Error in listen");
    exit(0);
  }

  int clientSocket0, clientSocket1;

  int clientLength0 = sizeof(clientAddress0);
  int clientLength1 = sizeof(clientAddress1);

  out_file = fopen(argv[1], "w");
  packet pkt0, pkt1, pkt_ack0, pkt_ack1;

  // 2 sockets for the 2 channels
  clientSocket0 =
      accept(serverSocket, (struct sockaddr *)&clientAddress0, &clientLength0);
  clientSocket1 =
      accept(serverSocket, (struct sockaddr *)&clientAddress1, &clientLength1);

  if (clientLength0 < 0 || clientLength1 < 0) {
    printf("Error in client socket");
    exit(0);
  }

  if (pthread_mutex_init(&lock, NULL) != 0) {
    printf("\n mutex init has failed\n");
    exit(1);
  }

  pthread_t tid0, tid1;

  // The 2 sockets run as 2 different threads
  pthread_create(&tid0, NULL, run_socket, (void *)&clientSocket0);
  pthread_create(&tid1, NULL, run_socket, (void *)&clientSocket1);

  pthread_join(tid0, NULL);
  pthread_join(tid1, NULL);

  while (buffer_size > 0) {
    fprintf(out_file, "%s", buffer[1].msg);
    delete_pkt(buffer, &buffer_size, buffer[1].seq_no);
  }

  close(serverSocket);
  fclose(out_file);
  pthread_exit(NULL);
  exit(0);
}
