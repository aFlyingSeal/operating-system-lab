// Tests for the sysinfo() system call.
//
// The structure is too large for the one-page user stack, so every snapshot
// lives in .bss (static) rather than on the stack.

#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "kernel/param.h"
#include "kernel/sysinfo.h"
#include "kernel/proc.h"
#include "kernel/fcntl.h"
#include "user/user.h"

static struct sysinfo si;
static struct sysinfo prev;
static int failures;

static void
check(int cond, const char *name)
{
  if(cond){
    printf("PASS: %s\n", name);
  } else {
    printf("FAIL: %s\n", name);
    failures++;
  }
}

static void
getinfo(struct sysinfo *s)
{
  if(sysinfo(s) < 0){
    printf("FAIL: sysinfo() returned -1\n");
    exit(1);
  }
}

static struct sysinfo_file *
findfile(struct sysinfo *s, int pid, int type)
{
  int i, j;

  for(i = 0; i < s->nfile; i++){
    if(type >= 0 && s->files[i].type != type)
      continue;
    for(j = 0; j < s->files[i].npid; j++)
      if(s->files[i].pids[j] == pid)
        return &s->files[i];
  }
  return 0;
}

static int
haspid(struct sysinfo_file *f, int pid)
{
  int i;

  for(i = 0; i < f->npid; i++)
    if(f->pids[i] == pid)
      return 1;
  return 0;
}

static struct sysinfo_proc *
findproc(struct sysinfo *s, int pid)
{
  int i;

  for(i = 0; i < s->nproc; i++)
    if(s->procs[i].pid == pid)
      return &s->procs[i];
  return 0;
}

static int
hasfd(struct sysinfo_proc *p, int fd)
{
  int i;

  for(i = 0; i < p->nfd; i++)
    if(p->fds[i] == fd)
      return 1;
  return 0;
}

static int
counttype(struct sysinfo *s, int type)
{
  int i, n = 0;

  for(i = 0; i < s->nfile; i++)
    if(s->files[i].type == type)
      n++;
  return n;
}

int
main(void)
{
  struct sysinfo_file *f, *cf;
  struct sysinfo_proc *me, *cp;
  int fd, fd2, child, a, b, i, j, k, ok, opened, failed;
  int p[2];
  int sp[2];
  char ch;

  // The kernel may not zero .bss in this lab configuration, so do not rely
  // on static initialization.
  failures = 0;

  printf("sysinfotest: sizeof(struct sysinfo) = %d bytes\n",
         (int)sizeof(struct sysinfo));

  // Baseline: the procs that must exist are init, sh and this test.
  getinfo(&si);
  printf("baseline: %d open files, %d processes\n", si.nfile, si.nproc);

  check(si.nfile > 0, "baseline: at least one open file");
  check(si.nproc >= 3, "baseline: init, sh and sysinfotest are listed");
  check(si.nfile <= NFILE, "baseline: nfile respects NFILE");
  check(si.nproc <= NPROC, "baseline: nproc respects NPROC");

  me = findproc(&si, getpid());
  check(me != 0, "baseline: this process is listed");
  check(me && me->state == RUNNING, "baseline: this process is RUNNING");
  check(me && me->nsched > 0, "baseline: this process has nsched > 0");
  check(me && me->parent_pid > 0, "baseline: this process has a parent");
  cp = me ? findproc(&si, me->parent_pid) : 0;
  check(cp != 0, "baseline: parent pid refers to a listed process");
  check(cp && cp->state == SLEEPING, "baseline: shell is SLEEPING in wait");
  check(me && hasfd(me, 0) && hasfd(me, 1) && hasfd(me, 2),
        "baseline: fds 0, 1 and 2 are open");

  ok = 1;
  for(i = 0; i < si.nproc; i++){
    if(si.procs[i].pid == 0 || si.procs[i].state == UNUSED)
      ok = 0;
    for(j = 0; j < i; j++)
      if(si.procs[i].pid == si.procs[j].pid)
        ok = 0;
  }
  check(ok, "baseline: no UNUSED entries and pids are unique");

  // Console: init opened it once and dup'ed it for fds 1 and 2; sh and this
  // test inherited it through fork, so it must have several references.
  cf = findfile(&si, getpid(), FD_DEVICE);
  check(cf != 0, "console: device file is listed");
  check(cf && cf->ref >= 3, "console: shared by at least three descriptors");
  check(cf && cf->readable && cf->writable, "console: readable and writable");
  check(cf && cf->npid >= 2, "console: referenced by several processes");

  // open()
  getinfo(&prev);
  fd = open("README", O_RDONLY);
  check(fd >= 0, "open: README opens");
  getinfo(&si);
  check(si.nfile == prev.nfile + 1, "open: creates exactly one file entry");

  f = findfile(&si, getpid(), FD_INODE);
  check(f != 0, "open: file entry lists this process");
  check(f && f->ref == 1, "open: reference count is 1");
  check(f && f->readable && !f->writable, "open: O_RDONLY mode is recorded");
  check(f && f->npid == 1, "open: one pid entry only");

  // dup()
  fd2 = dup(fd);
  check(fd2 >= 0 && fd2 != fd, "dup: returns a distinct descriptor");
  getinfo(&si);
  check(si.nfile == prev.nfile + 1, "dup: no new file entry");
  f = findfile(&si, getpid(), FD_INODE);
  check(f && f->ref == 2, "dup: reference count becomes 2");
  check(f && f->npid == 1, "dup: pid is not duplicated");
  me = findproc(&si, getpid());
  check(me && hasfd(me, fd) && hasfd(me, fd2),
        "dup: both descriptors are listed");

  close(fd2);
  getinfo(&si);
  f = findfile(&si, getpid(), FD_INODE);
  check(f && f->ref == 1, "close: reference count returns to 1");

  // fork(): the child inherits the descriptor table.
  child = fork();
  if(child == 0){
    sleep(50);
    exit(0);
  }
  check(child > 0, "fork: child is created");

  getinfo(&si);
  f = findfile(&si, getpid(), FD_INODE);
  check(f && f->ref == 2, "fork: child duplicates the open file");
  check(f && haspid(f, child), "fork: child pid is added to the file");
  cp = findproc(&si, child);
  check(cp != 0, "fork: child is listed");
  check(cp && cp->parent_pid == getpid(), "fork: parent pid is correct");
  check(cp && hasfd(cp, fd), "fork: child lists the inherited descriptor");

  wait(0);
  getinfo(&si);
  check(findproc(&si, child) == 0, "wait: child leaves the process list");
  f = findfile(&si, getpid(), FD_INODE);
  check(f && f->ref == 1, "wait: file reference count drops back to 1");

  close(fd);
  getinfo(&si);
  check(si.nfile == prev.nfile, "close: file table returns to baseline");

  // ZOMBIE: an exited child stays listed until its parent waits.
  child = fork();
  if(child == 0)
    exit(7);
  sleep(10);
  getinfo(&si);
  cp = findproc(&si, child);
  check(cp != 0 && cp->state == ZOMBIE, "zombie: unwaited child is ZOMBIE");
  wait(0);
  getinfo(&si);
  check(findproc(&si, child) == 0, "zombie: gone after wait");

  // SLEEPING: a child blocked reading an empty pipe.  Unlike sleep(), a pipe
  // wait is not woken on every tick, so the state stays SLEEPING while the
  // parent takes its snapshot.
  check(pipe(sp) == 0, "sleep: pipe for the blocking child");
  child = fork();
  if(child == 0){
    close(sp[1]);
    read(sp[0], &ch, 1);
    exit(0);
  }
  close(sp[0]);
  sleep(5);
  getinfo(&si);
  cp = findproc(&si, child);
  check(cp != 0 && cp->state == SLEEPING, "sleep: blocked child is SLEEPING");
  write(sp[1], "x", 1);
  wait(0);
  close(sp[1]);
  getinfo(&si);
  check(findproc(&si, child) == 0, "sleep: woken child is reaped");

  // pipe(): two FD_PIPE files, one for each end.
  getinfo(&prev);
  check(pipe(p) == 0, "pipe: creates a pipe");
  getinfo(&si);
  check(si.nfile == prev.nfile + 2, "pipe: creates two file entries");
  check(counttype(&si, FD_PIPE) == 2, "pipe: exactly two pipe files");
  ok = 1;
  for(i = 0; i < si.nfile; i++)
    if(si.files[i].type == FD_PIPE)
      if(si.files[i].ref != 1 || !haspid(&si.files[i], getpid()))
        ok = 0;
  check(ok, "pipe: each end has ref 1 and lists this process");
  close(p[0]);
  close(p[1]);
  getinfo(&si);
  check(si.nfile == prev.nfile, "pipe: closing removes both entries");

  // NOFILE: fill the descriptor table.
  getinfo(&prev);
  opened = failed = 0;
  for(i = 0; i < NOFILE + 4; i++){
    int x = open("README", O_RDONLY);
    if(x >= 0)
      opened++;
    else
      failed++;
  }
  check(opened == NOFILE - 3, "nofile: every free descriptor is used");
  check(failed == 7, "nofile: open fails once NOFILE is reached");
  getinfo(&si);
  me = findproc(&si, getpid());
  check(me && me->nfd == NOFILE, "nofile: process lists NOFILE descriptors");
  for(i = 3; i < NOFILE; i++)
    close(i);
  getinfo(&si);
  me = findproc(&si, getpid());
  check(me && me->nfd == 3, "nofile: closing extras leaves fds 0, 1, 2");
  check(si.nfile == prev.nfile, "nofile: file table returns to baseline");

  // Cross-checks between the file list and the process list.
  getinfo(&si);
  ok = 1;
  for(i = 0; i < si.nfile; i++)
    if(si.files[i].ref <= 0 || si.files[i].npid > NPROC ||
       si.files[i].npid <= 0)
      ok = 0;
  check(ok, "cross: every file has a positive ref and a pid list");

  ok = 1;
  for(i = 0; i < si.nproc; i++){
    if(si.procs[i].nfd <= 0)
      continue;
    k = 0;
    for(j = 0; j < si.nfile; j++)
      if(haspid(&si.files[j], si.procs[i].pid))
        k = 1;
    if(k == 0)
      ok = 0;
  }
  check(ok, "cross: every process with fds appears in the file list");

  // nsched must grow when the process is scheduled again.
  getinfo(&si);
  me = findproc(&si, getpid());
  a = me ? me->nsched : -1;
  sleep(2);
  getinfo(&si);
  me = findproc(&si, getpid());
  b = me ? me->nsched : -1;
  check(a >= 0 && b > a, "nsched: increases after a context switch");

  // A bad user pointer must be rejected instead of panicking.
  check(sysinfo((struct sysinfo *)MAXVA) == -1,
        "copyout: invalid user pointer is rejected");

  if(failures == 0)
    printf("ALL TESTS PASSED\n");
  else
    printf("%d TEST(S) FAILED\n", failures);
  exit(failures ? 1 : 0);
}
