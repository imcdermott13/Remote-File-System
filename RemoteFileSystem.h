//
// RemoteFileSystem.h
//
// Client-side remote (network) filesystem
//
// Skeleton provided by: Morris Bernstein
// Copyright 2019, Systems Deployment, LLC.
//
// Modified by: Ian McDermott

#if !defined(RemoteFileSystem_H)
#define RemoteFileSystem_H

#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>


class RemoteFileSystem {
 public:

  // File represents an open file object
  class File {
  public:
    // Destructor closes open file.
    ~File();

   ssize_t read(void *buf, size_t count);
   ssize_t write(void *buf, size_t count);
   off_t lseek(off_t offset, int whence);

  private:
    // Only RemoteFileSystem can open a file.
    friend class RemoteFileSystem;
    File(RemoteFileSystem* filesystem, const char *pathname, char *mode);

    // Disallow copy & assignment
    File(File const &) = delete;
    void operator=(File const &) = delete;
	
	int fd;
	RemoteFileSystem *filesystem;
	const char *pathname;
	char *mode;
  };

  // Connect to remote system.  Throw error if connection cannot be
  // made.
  RemoteFileSystem(char *host,
		   short port,
		   unsigned long auth_token,
		   struct timeval *timeout);

  // Disconnect
  ~RemoteFileSystem();

  // Return new open file object.  Client is responsible for
  // deleting.
  File *open(const char *pathname, char *mode);

  int chmod(const char *pathname, mode_t mode);
  int unlink(const char *pathname);
  int rename(const char *oldpath, const char *newpath);

 private:
  // File class may use private methods of the RemoteFileSystem to
  // implement their operations.  Alternatively, you can create a
  // separate Connection class that is local to your implementation.
  friend class File;
  int closefile(int fd);

  // Disallow copy & assignment
  RemoteFileSystem(RemoteFileSystem const &) = delete;
  void operator=(RemoteFileSystem const &) = delete;
  
  int sockid;
  unsigned long auth_token;
};


#endif
