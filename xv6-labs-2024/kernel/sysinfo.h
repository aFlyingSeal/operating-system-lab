// Shared data structures for the sysinfo() system call.
//
// The kernel fills one of these structures in its own memory and hands it to
// the user with copyout(), which copies raw bytes.  Keep the layout free of
// pointers and identical for both compilations (this header is the single
// definition used by kernel and user code).

#ifndef SYSINFO_H
#define SYSINFO_H

#include "param.h"

// Upper bounds of the fixed-size arrays below.  They match the sizes of the
// kernel tables, so a full snapshot always fits.
#define SYSINFO_MAX_FILES NFILE   // kernel/file.c: ftable.file[NFILE]
#define SYSINFO_MAX_PROCS NPROC   // kernel/proc.c: proc[NPROC]
#define SYSINFO_MAX_PIDS  NPROC   // one process cannot appear twice
#define SYSINFO_MAX_FDS   NOFILE  // kernel/proc.h: ofile[NOFILE]

// One open file description (a struct file with ref > 0).
struct sysinfo_file {
  int type;                         // FD_PIPE, FD_INODE or FD_DEVICE
  int readable;                     // 1 if the file may be read
  int writable;                     // 1 if the file may be written
  int ref;                          // number of descriptors referring to it
  int npid;                         // valid entries in pids[]
  int pids[SYSINFO_MAX_PIDS];       // processes referring to this file
};

// One process whose state is not UNUSED.
struct sysinfo_proc {
  int pid;
  int state;                        // enum procstate
  int parent_pid;                   // 0 when there is no parent
  int nsched;                       // times picked by scheduler()
  int nfd;                          // valid entries in fds[]
  int fds[SYSINFO_MAX_FDS];         // open descriptors
};

struct sysinfo {
  int nfile;
  struct sysinfo_file files[SYSINFO_MAX_FILES];
  int nproc;
  struct sysinfo_proc procs[SYSINFO_MAX_PROCS];
};

#endif
