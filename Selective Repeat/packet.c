#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>

int create_socket() {
  int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    printf("Error in opening a socket");
    exit(0);
  }
  return sock;
}

char *get_timestamp() {
  time_t rawtime;
  struct tm *timeinfo;
  struct timeval tv;

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  gettimeofday(&tv, NULL);

  char *time = (char *)malloc(sizeof(char) * 16);
  strftime(time, 16, "%H:%M:%S", timeinfo);
  char ms[7];
  sprintf(ms, ".%ld", tv.tv_usec);
  strcat(time, ms);
  return time;
}