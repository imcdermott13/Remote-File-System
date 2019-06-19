//
// marshaling.h
//
// struct for marshalling data for a remote filesystem
//
// Created by: Ian McDermott

#if !defined(Marshalling_H)
#define Marshalling_H

#define BUFSIZE 4078

enum Func { CONNECT, OPEN, CLOSE, READ, WRITE, LSEEK, CHMOD, UNLINK, RENAME };

struct comm {
  unsigned long auth;
  unsigned long seqnum;
  Func func;
  int fd;
  int len;
  int whence;
  char info[BUFSIZE];
  char mode[BUFSIZE];
};

#endif