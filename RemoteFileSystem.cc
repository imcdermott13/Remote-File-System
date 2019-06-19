//
// RemoteFileSystem.cc
//
// Created by: Ian McDermott

#include <iostream>
#include <netdb.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string>
#include <string.h>
#include <sys/syscall.h>
#include <linux/random.h>
#include <errno.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "RemoteFileSystem.h"
#include "marshalling.h"
using namespace std;

RemoteFileSystem::File::File(RemoteFileSystem* filesystem, const char *pathname,
                             char *mode) : pathname(pathname) {
  this->filesystem = filesystem;
  this->mode = mode;
  this->fd = 0;
}

RemoteFileSystem::File::~File() {
  this->filesystem->closefile(this->fd);
}

ssize_t RemoteFileSystem::File::read(void *buf, size_t count) {
  // Recursively reads from server if count is larger than BUFSIZE
  int over = 0;
  if (count > BUFSIZE) {
    over = this->read(buf, count - BUFSIZE);
      if (over < 0) return -1;
  }

  unsigned long seq = 0;
  if (syscall(SYS_getrandom, &seq, sizeof(unsigned long), 0) < 0) {
    return -1;
  }
  struct comm to_send = {this->filesystem->auth_token, seq, READ, fd,
                         (int)count - over, 0, "\0", "\0"};
  struct comm to_recv = {0, 0, CONNECT, 0,
                         0, 0, "\0", "\0"};
  
  for (int tries = 0; tries < 2; tries++) {
    if (send(this->filesystem->sockid, &to_send, sizeof(comm), 0) < 0) {
	  return -1;
	}
	errno = 0;
	if (recv(this->filesystem->sockid, &to_recv, sizeof(comm), 0) < 0) {
	  if (errno == EAGAIN) continue;
	  return -1;
	}
	if (to_recv.seqnum != seq) {
	  if (tries == 0) continue;
	  return -1;
	} else break;
  }
  int i;
  for (i = 0; i < to_recv.len && i < BUFSIZE; i++)
	  *((char *)buf + i + over) = to_recv.info[i];
  // returns count or -1 if server returned error
  return to_recv.len >= 0 ? i + over : -1;
}

ssize_t RemoteFileSystem::File::write(void *buf, size_t count) {
  // Recursively write to server if count is larger than BUFSIZE
  int over = 0;
  if (count > BUFSIZE) {
    over = this->write(buf, count - BUFSIZE);
	if (over < 0) return -1;
  }

  unsigned long seq = 0;
  if (syscall(SYS_getrandom, &seq, sizeof(unsigned long), 0) < 0) {
    return -1;
  }
  struct comm to_send = {this->filesystem->auth_token, seq, WRITE, fd,
                         (int)count - over, 0, "\0", "\0"};
  strcpy(to_send.info, (char *)buf + over);
  struct comm to_recv = {0, 0, CONNECT, 0,
                         0, 0, "\0", "\0"};
  
  for (int tries = 0; tries < 2; tries++) {
    if (send(this->filesystem->sockid, &to_send, sizeof(comm), 0) < 0) {
	  return -1;
	}
	errno = 0;
	if (recv(this->filesystem->sockid, &to_recv, sizeof(comm), 0) < 0) {
	  if (errno == EAGAIN) continue;
	  return -1;
	}
	if (to_recv.seqnum != seq) {
	  if (tries == 0) continue;
	  return -1;
	} else break;
  }
  // returns count or -1 if server returned error
  return to_recv.len >= 0 ? to_recv.len + over : -1;
}

off_t RemoteFileSystem::File::lseek(off_t offset, int whence) {
  unsigned long seq = 0;
  if (syscall(SYS_getrandom, &seq, sizeof(unsigned long), 0) < 0) {
    return -1;
  }
  struct comm to_send = {this->filesystem->auth_token, seq, LSEEK, fd,
                         (int)offset, whence, "\0", "\0"};
  struct comm to_recv = {0, 0, CONNECT, 0,
                         0, 0, "\0", "\0"};
  
  for (int tries = 0; tries < 2; tries++) {
    if (send(this->filesystem->sockid, &to_send, sizeof(comm), 0) < 0) {
	  return -1;
	}
	errno = 0;
	if (recv(this->filesystem->sockid, &to_recv, sizeof(comm), 0) < 0) {
	  if (errno == EAGAIN) continue;
	  return -1;
	}
	if (to_recv.seqnum != seq) {
	  if (tries == 0) continue;
	  return -1;
	} else break;
  }
  return to_recv.len;
}


RemoteFileSystem::RemoteFileSystem(char *host,
                                   short port,
                                   unsigned long auth_token,
                                   struct timeval *timeout) {
  int csd = socket(AF_INET, SOCK_DGRAM, 0);
  if (csd < 0) {
    cerr << "Issue creating socket" << endl;
    exit(EXIT_FAILURE);
  }
  if (timeout != NULL) {
    if (setsockopt(csd, SOL_SOCKET, SO_RCVTIMEO,
                   (char *)timeout, sizeof(struct timeval)) < 0) {
	  cerr << "Issue setting timeout" << endl;
	  exit(EXIT_FAILURE);
	}
  }
  
  struct addrinfo hints;
  bzero(&hints, sizeof(struct addrinfo));
  hints.ai_flags = 0;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = 0;
  hints.ai_addrlen = 0;
  hints.ai_addr = NULL;
  hints.ai_canonname = NULL;
  hints.ai_next = NULL;
  
  struct addrinfo *sinfo;
  if (getaddrinfo(host, std::to_string(port).c_str(), &hints, &sinfo) != 0) {
    cerr << "Issue finding server" << endl;
    exit(EXIT_FAILURE);
  }

  if (connect(csd, sinfo->ai_addr, sizeof(struct sockaddr_in)) < 0) {
    cerr << "Issue connecting to server" << endl;
    exit(EXIT_FAILURE);
  }

  unsigned long seq = 0;
  if (syscall(SYS_getrandom, &seq, sizeof(unsigned long), 0) < 0) {
    cerr << "Issue calling getrandom" << endl;
    exit(EXIT_FAILURE);
  }
  struct comm to_send = {auth_token, seq, CONNECT, 0, 0, 0, "\0", "\0"};
  struct comm to_recv = {0, 0, CONNECT, 0,
                         0, 0, "\0", "\0"};
  
  for (int tries = 0; tries < 2; tries++) {
    if (send(csd, &to_send, sizeof(comm), 0) < 0) {
      cerr << "Issue sending message" << endl;
      exit(EXIT_FAILURE);
    }
    errno = 0;
    if (recv(csd, &to_recv, sizeof(comm), 0) < 0) {
      if (errno == EAGAIN && tries == 0) continue;
      cerr << "Issue recieving response" << endl;
      exit(EXIT_FAILURE);
      }
    if (to_recv.seqnum != seq) {
      if (tries == 0) continue;
      cerr << "Recieved incorrect response" << endl;
      exit(EXIT_FAILURE);
    } else break;
  }
  
  this->auth_token = auth_token;
  this->sockid = csd;
}

RemoteFileSystem::~RemoteFileSystem() {
  close(this->sockid);
}

RemoteFileSystem::File* RemoteFileSystem::open(const char *pathname,
                                                   char *mode) {
  unsigned long seq = 0;
  if (syscall(SYS_getrandom, &seq, sizeof(unsigned long), 0) < 0) {
    return NULL;
  }
  struct comm to_send = {this->auth_token, seq, OPEN, 0, 0, 0, "\0", "\0"};
  strcpy(to_send.info, pathname);
  strcpy(to_send.mode, mode);
  struct comm to_recv = {0, 0, CONNECT, 0,
                         0, 0, "\0", "\0"};

  
  for (int tries = 0; tries < 2; tries++) {
    if (send(this->sockid, &to_send, sizeof(comm), 0) < 0) {
	  return NULL;
	}
	errno = 0;
	if (recv(this->sockid, &to_recv, sizeof(comm), 0) < 0) {
	  if (errno == EAGAIN) continue;
	  return NULL;
	}
	if (to_recv.seqnum != seq) {
	  if (tries == 0) continue;
	  return NULL;
	} else break;
  }
  File *file = new File(this, pathname, mode);
  file->fd = to_recv.fd;
  return file;
}

int RemoteFileSystem::closefile(int fd) {
  unsigned long seq = 0;
  if (syscall(SYS_getrandom, &seq, sizeof(unsigned long), 0) < 0) {
    return -1;
  }
  struct comm to_send = {this->auth_token, seq, CLOSE, fd, 0, 0, "\0", "\0"};
  struct comm to_recv = {0, 0, CONNECT, 0,
                         0, 0, "\0", "\0"};
  
  for (int tries = 0; tries < 2; tries++) {
    if (send(this->sockid, &to_send, sizeof(comm), 0) < 0) {
	  return -1;
	}
	errno = 0;
	if (recv(this->sockid, &to_recv, sizeof(comm), 0) < 0) {
	  if (errno == EAGAIN) continue;
	  return -1;
	}
	if (to_recv.seqnum != seq) {
	  if (tries == 0) continue;
	  return -1;
	} else break;
  }
  return 0;
}

int RemoteFileSystem::chmod(const char *pathname, mode_t mode) {
  unsigned long seq = 0;
  if (syscall(SYS_getrandom, &seq, sizeof(unsigned long), 0) < 0) {
    return -1;
  }
  struct comm to_send = {this->auth_token, seq, CHMOD, 0,
                         0, (int)mode, "\0", "\0"};
  strcpy(to_send.info, pathname);
  struct comm to_recv = {0, 0, CONNECT, 0,
                         0, 0, "\0", "\0"};
  
  for (int tries = 0; tries < 2; tries++) {
    if (send(this->sockid, &to_send, sizeof(comm), 0) < 0) {
	  return -1;
	}
	errno = 0;
	if (recv(this->sockid, &to_recv, sizeof(comm), 0) < 0) {
	  if (errno == EAGAIN) continue;
	  return -1;
	}
	if (to_recv.seqnum != seq) {
	  if (tries == 0) continue;
	  return -1;
	} else break;
  }
  return to_recv.fd;
}

int RemoteFileSystem::unlink(const char *pathname) {
  unsigned long seq = 0;
  if (syscall(SYS_getrandom, &seq, sizeof(unsigned long), 0) < 0) {
    return -1;
  }
  struct comm to_send = {this->auth_token, seq, UNLINK, 0, 0, 0, "\0", "\0"};
  strcpy(to_send.info, pathname);
  struct comm to_recv = {0, 0, CONNECT, 0,
                         0, 0, "\0", "\0"};
  
  for (int tries = 0; tries < 2; tries++) {
    if (send(this->sockid, &to_send, sizeof(comm), 0) < 0) {
	  return -1;
	}
	errno = 0;
	if (recv(this->sockid, &to_recv, sizeof(comm), 0) < 0) {
	  if (errno == EAGAIN) continue;
	  return -1;
	}
	if (to_recv.seqnum != seq) {
	  if (tries == 0) continue;
	  return -1;
	} else break;
  }
  return to_recv.fd;
}

int RemoteFileSystem::rename(const char *oldpath, const char *newpath) {
  unsigned long seq = 0;
  if (syscall(SYS_getrandom, &seq, sizeof(unsigned long), 0) < 0) {
    return -1;
  }
  struct comm to_send = {this->auth_token, seq, RENAME, 0,
                         0, 0, "\0", "\0"};
  strcpy(to_send.info, oldpath);
  strcpy(to_send.mode, newpath);
  struct comm to_recv = {0, 0, CONNECT, 0,
                         0, 0, "\0", "\0"};
  
  for (int tries = 0; tries < 2; tries++) {
    if (send(this->sockid, &to_send, sizeof(comm), 0) < 0) {
      return -1;
    }
    errno = 0;
    if (recv(this->sockid, &to_recv, sizeof(comm), 0) < 0) {
      if (errno == EAGAIN) continue;
      return -1;
    }
    if (to_recv.seqnum != seq) {
      if (tries == 0) continue;
      return -1;
    } else break;
  }
  return to_recv.fd;
}
