#include "packet.h"
#include "priority_queue.h"
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define TIMEOUT 2

bool is_last = false;
bool timedout;
packet buffer[WINDOWSIZE]; // Sliding window is implemented as a priority queue
                           // of non-acknowledged packets
int buffer_size = 0;
pthread_mutex_t lock;
FILE *log_file;

typedef struct thread_args_sender {
  int sock;
  struct sockaddr_in dest0;
  struct sockaddr_in dest1;
  FILE *fp;
} thread_args_sender;

void *sender(void *args) {

  thread_args_sender *temp = (thread_args_sender *)args;
  int sock = temp->sock;
  FILE *fp = temp->fp;
  struct sockaddr_in dest0 = temp->dest0;
  struct sockaddr_in dest1 = temp->dest1;

  struct sockaddr_in dest;
  packet pkt;
  int next_seq_num =
      0; // The cumulative sequence number of the next new packet to be sent

  do {

    // The case where the buffer is not completely filled and the the input file
    // is yet not read completely
    if (buffer_size < WINDOWSIZE - 1 && is_last == false) {

      int num = fread(pkt.msg, 1, PACKETSIZE - 1, fp);
      pkt.size = num;
      pkt.seq_no = next_seq_num;
      pkt.is_data = true;
      is_last = pkt.is_last =
          (num != PACKETSIZE -
                      1); // If a packet is the last packet to be created,
                          // then size of data read is not equal to packet size
      pkt.channel_id =
          pkt.seq_no % 2; // Channel allocated based on whether
                          // packet's sequence number is odd or even
      next_seq_num = (next_seq_num + 1);
      pkt.msg[num] = '\0';

      // Inserts into the priority queue
      // Priority queue is thread synchronised with the reciever
      pthread_mutex_lock(&lock);
      heap_insert(buffer, &buffer_size, pkt);
      pthread_mutex_unlock(&lock);

      // Destination decided based on channel_id
      if (pkt.channel_id == 0) {
        dest = dest0;
        printf("CLIENT\tS\t%s\tDATA\t%d\tCLIENT\tRELAY1\n", get_timestamp(),
               pkt.seq_no);
        fprintf(log_file, "CLIENT\tS\t%s\tDATA\t%d\tCLIENT\tRELAY1\n",
                get_timestamp(), pkt.seq_no);
      } else {
        dest = dest1;
        printf("CLIENT\tS\t%s\tDATA\t%d\tCLIENT\tRELAY2\n", get_timestamp(),
               pkt.seq_no);
        fprintf(log_file, "CLIENT\tS\t%s\tDATA\t%d\tCLIENT\tRELAY2\n",
                get_timestamp(), pkt.seq_no);
      }
      sendto(sock, &pkt, sizeof(pkt), 0, (struct sockaddr *)&dest,
             sizeof(dest));
    }

    // The case where priority queue is full or the input file is read
    // completely. Packets resent if the global variable timedout is true. Value
    // of timedout can change mid-way on the reciever thread.
    else {

      // Priority queue is thread synchronised with the reciever
      pthread_mutex_lock(&lock);
      for (int i = 1; i <= buffer_size; i++) {
        if (timedout == true) {
          if (buffer[i].channel_id == 0) {
            dest = dest0;
            printf("CLIENT\tRE\t%s\tDATA\t%d\tCLIENT\tRELAY1\n",
                   get_timestamp(), buffer[i].seq_no);
            fprintf(log_file, "CLIENT\tRE\t%s\tDATA\t%d\tCLIENT\tRELAY1\n",
                    get_timestamp(), buffer[i].seq_no);
          } else {
            dest = dest1;
            printf("CLIENT\tRE\t%s\tDATA\t%d\tCLIENT\tRELAY2\n",
                   get_timestamp(), buffer[i].seq_no);
            fprintf(log_file, "CLIENT\tRE\t%s\tDATA\t%d\tCLIENT\tRELAY2\n",
                    get_timestamp(), buffer[i].seq_no);
          }

          sendto(sock, &buffer[i], sizeof(buffer[i]), 0,
                 (struct sockaddr *)&dest, sizeof(dest));
        } else {
          break;
        }
      }
      pthread_mutex_unlock(&lock);
      sleep(TIMEOUT);
    }

  } while (!(is_last == true && buffer_size == 0));
  // Exits when whole input file is read and the priority queue is empty
}

void *reciever(void *sock_ptr) {

  int *temp = (int *)sock_ptr;
  int sock = *temp;
  packet pkt;
  struct sockaddr_in dest;

  while (true) {
    int size = sizeof(struct sockaddr_in);
    int temp =
        recvfrom(sock, &pkt, sizeof(pkt), 0, (struct sockaddr *)&dest, &size);

    // Timeout case
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      errno = -1;
      timedout = true;
      printf("CLIENT\tTO\t%s\t---\t---\t----\tCLIENT\n", get_timestamp());
      fprintf(log_file, "CLIENT\tTO\t%s\t---\t---\t----\tCLIENT\n",
              get_timestamp());
      continue;
    }
    if (temp < 0) {
      // printf("Not recieved properly\n");
      exit(0);
    }

    if (pkt.channel_id == 0) {
      printf("CLIENT\tR\t%s\tACK\t%d\tRELAY1\tCLIENT\n", get_timestamp(),
             pkt.seq_no);
      fprintf(log_file, "CLIENT\tR\t%s\tACK\t%d\tRELAY1\tCLIENT\n",
              get_timestamp(), pkt.seq_no);
    } else {
      printf("CLIENT\tR\t%s\tACK\t%d\tRELAY2\tCLIENT\n", get_timestamp(),
             pkt.seq_no);
      fprintf(log_file, "CLIENT\tR\t%s\tACK\t%d\tRELAY2\tCLIENT\n",
              get_timestamp(), pkt.seq_no);
    }
    timedout = false;

    // Packet deleted from priority queue when acknowledgement recieved.
    // Priority queue is thread synchronised with the sender
    pthread_mutex_lock(&lock);
    if (buffer_size > 0) {
      delete_pkt(buffer, &buffer_size, pkt.seq_no);
    }
    pthread_mutex_unlock(&lock);
  }
}

int main(int argc, char *argv[]) {

  is_last = false;
  log_file = fopen(LOG_FILE, "a");
  FILE *inp_file = fopen(argv[1], "r");
  if (inp_file == NULL) {
    printf("Input file %s not found in the current directory. Check and try "
           "again\n",
           argv[1]);
  }
  timedout = false;
  srand(time(NULL));
  heap_init(buffer, &buffer_size);

  if (pthread_mutex_init(&lock, NULL) != 0) {
    printf("\n mutex init has failed\n");
    exit(0);
  }

  struct sockaddr_in relayAddr0, relayAddr1, my_addr;

  // Creates socket and binds the socket to CLIENT_IP and CLIENT_PORT
  int sock = create_socket();
  memset((char *)&my_addr, 0, sizeof(my_addr));
  my_addr.sin_family = AF_INET;
  my_addr.sin_addr.s_addr = INADDR_ANY;
  my_addr.sin_port = htons(CLIENT_PORT);
  my_addr.sin_addr.s_addr = inet_addr(CLIENT_IP);
  int bnd = bind(sock, (struct sockaddr *)&my_addr, sizeof(struct sockaddr_in));

  if (bnd != 0) {
    printf("Did not bind properly\n");
    exit(0);
  }

  // Address of Relay1
  memset((char *)&relayAddr0, 0, sizeof(relayAddr0));
  relayAddr0.sin_family = AF_INET;
  relayAddr0.sin_port = htons(RELAY_PORT0);
  relayAddr0.sin_addr.s_addr = inet_addr(RELAY_IP);

  // Address of Relay2
  memset((char *)&relayAddr1, 0, sizeof(relayAddr1));
  relayAddr1.sin_family = AF_INET;
  relayAddr1.sin_port = htons(RELAY_PORT1);
  relayAddr1.sin_addr.s_addr = inet_addr(RELAY_IP);

  // Sets the socket options so that recvfrom becomes non-blocking and max. time
  // it waits for is TIMEOUT
  struct timeval tv;
  tv.tv_sec = TIMEOUT;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv,
             sizeof(struct timeval));

  pthread_t tid0, tid1;

  thread_args_sender args0;
  args0.dest0 = relayAddr0;
  args0.dest1 = relayAddr1;
  args0.sock = sock;
  args0.fp = inp_file;

  // The sender and reciever run as 2 seperate threads
  pthread_create(&tid0, NULL, sender, (void *)&args0);
  pthread_create(&tid1, NULL, reciever, (void *)&sock);

  pthread_join(tid0, NULL);

  fclose(log_file);
  fclose(inp_file);
  close(sock);
  pthread_mutex_destroy(&lock);
  pthread_exit(NULL);

  return 0;
}