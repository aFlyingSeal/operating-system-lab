// Tests for the page-table syscall added in Lab 2.
//
// pgaccess(base, len, mask) reports, one bit per page, which of the len pages
// starting at base have the RISC-V accessed bit set, and clears that bit so
// the next call reports only new accesses.

#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "user/user.h"

// One uint64 bitmask, so this is also the kernel's upper bound.
#define MAXPAGES 64

#define NPAGES 32

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

// The bits a mask would have if only the given pages were accessed.
static uint64
bitof(int page)
{
  return 1UL << page;
}

// vmprint() asks the kernel to dump the calling process's page table.  The
// dump goes to the console, so a user program can only check that the call
// succeeds and that the table the kernel walked is still intact afterwards.
static void
vmprint_test(void)
{
  char *buf;
  uint64 mask;

  printf("pgtbltest: asking the kernel for this process's page table\n");
  check(vmprint() == 0, "vmprint: dumps the page table on demand");

  buf = sbrk(PGSIZE);
  check(buf != (char*)-1, "vmprint: the heap is still usable afterwards");
  if(buf == (char*)-1)
    return;

  check(pgaccess(buf, 1, &mask) == 0,
        "vmprint: the page table is still walkable");
  buf[0] = 'v';
  check(pgaccess(buf, 1, &mask) == 0 && mask == bitof(0),
        "vmprint: the new page still records accesses");
  sbrk(-PGSIZE);
}

// Touching a page between calls is what sets PTE_A in the first place.
static void
pgaccess_test(void)
{
  char *buf;
  uint64 mask, again;

  buf = sbrk(NPAGES * PGSIZE);
  check(buf != (char*)-1, "pgaccess: heap grows by NPAGES pages");
  if(buf == (char*)-1)
    return;

  // The pages were just mapped by sbrk(); the kernel cleared them through the
  // direct map, so no access bit is set yet.  One call makes sure, whatever
  // the previous state was.
  check(pgaccess(buf, NPAGES, &mask) == 0, "pgaccess: first call succeeds");
  check(mask == 0, "pgaccess: freshly mapped pages are not reported");

  // Write one page, read another: both count as an access.
  buf[0] = 'a';
  check(buf[5 * PGSIZE] == 0, "pgaccess: untouched page reads as zero");

  check(pgaccess(buf, NPAGES, &mask) == 0, "pgaccess: second call succeeds");
  check(mask == (bitof(0) | bitof(5)),
        "pgaccess: exactly the written and read pages are reported");

  check(pgaccess(buf, NPAGES, &again) == 0, "pgaccess: third call succeeds");
  check(again == 0, "pgaccess: the previous call cleared the access bits");

  // The mask must be written to user memory, not the kernel's own copy.
  mask = 0;
  buf[NPAGES * PGSIZE - 1] = 'z';
  check(pgaccess(buf, NPAGES, &mask) == 0, "pgaccess: last page call succeeds");
  check(mask == bitof(NPAGES - 1), "pgaccess: last page of the range is reported");

  // A shorter range only looks at its own pages.
  buf[1] = 'b';
  check(pgaccess(buf, 4, &mask) == 0, "pgaccess: short range call succeeds");
  check(mask == bitof(0), "pgaccess: short range ignores pages past len");

  // Clear what is left before the error cases.
  check(pgaccess(buf, NPAGES, &mask) == 0, "pgaccess: range cleared");

  // An invalid mask pointer must not clear the access bits.
  buf[0] = 'c';
  check(pgaccess(buf, 1, (void*)MAXVA) == -1,
        "pgaccess: bad mask pointer is rejected");
  check(pgaccess(buf, 1, &mask) == 0,
        "pgaccess: call after bad mask succeeds");
  check(mask == bitof(0),
        "pgaccess: bad mask does not clear access bits");

  // The base must be page-aligned.
  buf[0] = 'd';
  check(pgaccess(buf + 1, 1, &mask) == -1,
        "pgaccess: unaligned base is rejected");
  check(pgaccess(buf, 1, &mask) == 0,
        "pgaccess: call after unaligned base succeeds");
  check(mask == bitof(0),
        "pgaccess: unaligned base does not clear access bits");

  // Unmapped memory: sbrk() shrink takes the pages away again.
  sbrk(-NPAGES * PGSIZE);
  check(pgaccess(buf, NPAGES, &mask) == -1,
        "pgaccess: unmapped range is rejected");

  // Invalid arguments.
  check(pgaccess(buf, 0, &mask) == -1, "pgaccess: len 0 is rejected");
  check(pgaccess(buf, -1, &mask) == -1, "pgaccess: negative len is rejected");
  check(pgaccess(buf, MAXPAGES + 1, &mask) == -1,
        "pgaccess: more than MAXPAGES pages is rejected");
  check(pgaccess((void*)MAXVA, 1, &mask) == -1,
        "pgaccess: virtual address beyond MAXVA is rejected");
}

int
main(int argc, char *argv[])
{
  printf("pgtbltest: page-table tests\n");

  failures = 0;
  pgaccess_test();
  vmprint_test();

  if(failures == 0)
    printf("ALL TESTS PASSED\n");
  else
    printf("%d TEST(S) FAILED\n", failures);
  exit(failures ? 1 : 0);
}
