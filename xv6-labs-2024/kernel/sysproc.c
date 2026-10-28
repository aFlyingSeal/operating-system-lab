#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_trace(void)
{
  int mask;
  argint(0, &mask);
  myproc()->trace_mask = mask;
  return 0;
}

struct file;

extern struct proc proc[NPROC];

// Snapshot buffer shared by all sysinfo() calls.  The structure is ~32 KB,
// far too large for the 4 KB kernel stack, so it lives in BSS and is
// protected by snapshot_lock instead.
static struct sysinfo snapshot;
static struct spinlock snapshot_lock;
static struct file *fileptr[NFILE];

void
sysinfoinit(void)
{
  initlock(&snapshot_lock, "sysinfo");
}

// Record pid in sf->pids[] at most once.
static void
addpid(struct sysinfo_file *sf, int pid)
{
  int i;

  for(i = 0; i < sf->npid; i++)
    if(sf->pids[i] == pid)
      return;
  if(sf->npid < SYSINFO_MAX_PIDS)
    sf->pids[sf->npid++] = pid;
}

// Fill info with a best-effort snapshot: first the open-file table, then the
// process table.  The two tables are never locked at the same time, which
// avoids the p->lock -> ftable.lock order used by fork() when it duplicates
// descriptors.  Caller must hold snapshot_lock.
static void
collect(struct sysinfo *info)
{
  struct proc *p;
  int i, j, n;

  // Phase 1: open files.
  info->nfile = filesnapshot(info->files, fileptr, SYSINFO_MAX_FILES);

  // Phase 2: processes, attributing each descriptor to the file snapshot.
  n = 0;
  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->state != UNUSED && n < SYSINFO_MAX_PROCS){
      info->procs[n].pid = p->pid;
      info->procs[n].state = p->state;
      info->procs[n].parent_pid = p->parent ? p->parent->pid : 0;
      info->procs[n].nsched = p->nsched;
      info->procs[n].nfd = 0;
      for(i = 0; i < NOFILE; i++){
        if(p->ofile[i] == 0)
          continue;
        info->procs[n].fds[info->procs[n].nfd++] = i;
        for(j = 0; j < info->nfile; j++){
          if(fileptr[j] == p->ofile[i]){
            addpid(&info->files[j], p->pid);
            break;
          }
        }
      }
      n++;
    }
    release(&p->lock);
  }
  info->nproc = n;
}

// Fill a struct sysinfo describing open files and processes and copy it to
// the user pointer in arg 0.
uint64
sys_sysinfo(void)
{
  struct proc *p = myproc();
  uint64 addr;
  int r;

  argaddr(0, &addr);

  acquire(&snapshot_lock);
  collect(&snapshot);
  r = copyout(p->pagetable, addr, (char *)&snapshot, sizeof(snapshot));
  release(&snapshot_lock);

  return r < 0 ? -1 : 0;
}
