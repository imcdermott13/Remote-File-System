//
// RemoteFileServer.cc
//
// Created by: Ian McDermott

#include <iostream>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string>
#include <string.h>

#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "marshalling.h"
using namespace std;

int main(int argc, char* argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s port\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  int ssd = socket(AF_INET, SOCK_DGRAM, 0);
  if (ssd < 0) {
    cerr << "Issue creating socket" << endl;
    exit(EXIT_FAILURE);
  }
  
  struct sockaddr_in saddr;
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);
  saddr.sin_port = htons(atoi(argv[1]));

  if (bind(ssd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
    cerr << "Issue binding socket" << endl;
    exit(EXIT_FAILURE);
  }
  
  while (true) {
    struct sockaddr_in caddr;
    socklen_t len = sizeof(caddr);
	
    struct comm to_recv = {0, 0, CONNECT, 0,
                           0, 0, "\0", "\0"};
    // Wait for packet to be sent
    if (recvfrom(ssd, &to_recv, sizeof(comm), 0,
                 (struct sockaddr *)&caddr, &len) < 0) {
      cerr << "Issue receiving packet" << endl;
      continue;
    }
    // TODO: Keep and check a hash of valid auths
    struct comm to_send = {to_recv.auth, to_recv.seqnum, to_recv.func, 0,
                           0, 0, "\0", "\0"};
    // Determine what to do with packet, then perform computations
    if (to_recv.func == CONNECT) {
    } else if (to_recv.func == OPEN) {
      int mode;
      if (to_recv.mode[0] == 'r' && to_recv.mode[1] == '\0') 
        mode = O_RDONLY;
      else if (to_recv.mode[0] == 'w' && to_recv.mode[1] == '\0')
        mode = O_WRONLY;
      else if ((to_recv.mode[0] == 'r' || to_recv.mode[0] == 'w')
                && to_recv.mode[1] == '+')
        mode = O_RDWR;
      else
	    mode = -10;
    
      if (mode == -10)
        to_send.fd = -1;
      else
        to_send.fd = open(to_recv.info, mode);
    } else if (to_recv.func == CLOSE) {
      to_send.fd = close(to_recv.fd);
    } else if (to_recv.func == READ) {
      char buf[BUFSIZE];
      to_send.len = read(to_recv.fd, buf, to_recv.len);
      strcpy(to_send.info, buf);
    } else if (to_recv.func == WRITE) {
      to_send.len = write(to_recv.fd, to_recv.info, to_recv.len);
    } else if (to_recv.func == LSEEK) {
      to_send.len = lseek(to_recv.fd, to_recv.len, to_recv.whence);
    } else if (to_recv.func == CHMOD) {
      to_send.fd = chmod(to_recv.info, to_recv.whence);
    } else if (to_recv.func == UNLINK) {
      to_send.fd = unlink(to_recv.info);
    } else if (to_recv.func == RENAME) {
      to_send.fd = rename(to_recv.info, to_recv.mode);
    } else {
      to_send.fd = -1;
      to_send.len = -1;
    }
  
    // Send response to client
    if (sendto(ssd, &to_send, sizeof(comm), 0,
	           (struct sockaddr *)&caddr, len) < 0) {
      cerr << "Issue sending packet" << endl;
      continue;
    }
  }
  return 0;
}
