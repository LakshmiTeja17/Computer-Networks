#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

int create_socket() {
  int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock < 0) {
    printf("Error in opening a socket");
    exit(0);
  }
  return sock;
}