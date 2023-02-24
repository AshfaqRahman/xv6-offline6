// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

// cow
uint64 count_of_references[PHYSTOP / PGSIZE];

struct run
{
  struct run *next;
};

struct
{
  struct spinlock lock;
  struct run *freelist;
} kmem;

void kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  struct run *r;
  // if (count_of_references[(uint64)pa / PGSIZE] == 0)
  // {
  //   // printf("Invalid reference count\n");
  //   return;
  // }

  if (count_of_references[(uint64)pa / PGSIZE] > 1)
  {
    count_of_references[(uint64)pa / PGSIZE]--;
    if (count_of_references[(uint64)pa / PGSIZE])
      return;
  }

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  count_of_references[(uint64)r / PGSIZE] = 1;
  return (void *)r;
}


void count_reference_increase(uint64 pa) {
  if(count_of_references[pa / PGSIZE] == 0)
    panic("Invalid reference count\n");
  acquire(&kmem.lock);
  count_of_references[pa / PGSIZE]++;
  release(&kmem.lock);
}

void count_reference_decrease(uint64 pa) {
  if(count_of_references[pa / PGSIZE] == 0)
    panic("Invalid reference count\n");
  acquire(&kmem.lock);
  count_of_references[pa / PGSIZE]--;
  release(&kmem.lock);
  if(count_of_references[pa / PGSIZE] == 0)
    kfree((void *)pa);
}