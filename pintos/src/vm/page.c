#include "vm/page.h"
#include <stdio.h>
#include <string.h>
#include "vm/frame.h"
#include "vm/swap.h"
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "lib/debug.h"

/* Maximum size of process stack, in bytes. */
/* Right now it is 1 megabyte. */
#define STACK_MAX (1024 * 1024)

/* Destroys a page, which must be in the current process's
   page table.  Used as a callback for hash_destroy(). */
static void
destroy_page (struct hash_elem *p_, void *aux UNUSED)
{
  struct page *p = hash_entry (p_, struct page, hash_elem);
  frame_lock (p);
  if (p->frame)
    frame_free (p->frame);
  free (p);
}

/* Destroys the current process's page table. */
void
page_exit (void)
{
  struct hash *h = thread_current ()->pages;
  if (h != NULL)
    hash_destroy (h, destroy_page);
}

/* Returns the page containing the given virtual ADDRESS,
   or a null pointer if no such page exists.
   Allocates stack pages as necessary. */
static struct page *
page_for_addr (const void *address)
{
  // TODO: Round addr down to page boundary with pg_round_down()
  // TODO: Construct a temporary vm_page with this addr
  // TODO: Use hash_find to look it up
  // TODO: Return the found vm_page (or NULL if not found)

   /* Round address down to page boundary */
  void *page_addr = pg_round_down (address);
  
  /* Create a temporary page for hash lookup */
  struct page p;
  p.addr = page_addr;
  
  /* Look up the page in the hash table */
  struct hash_elem *e = hash_find (thread_current ()->pages, &p.hash_elem);
  
  /* Return the found page, or NULL if not found */
  if (e != NULL)
    return hash_entry (e, struct page, hash_elem);
  else
    return NULL;
}

/* Locks a frame for page P and pages it in.
   Returns true if successful, false on failure. */
static bool
do_page_in (struct page *p)
{
  /* Get a frame for the page. */
  p->frame = frame_alloc_and_lock (p);
  if (p->frame == NULL)
    return false;

  /* Copy data into the frame. */
  if (p->sector != (block_sector_t) -1)
    {
      /* Get data from swap. */
      swap_in (p);
    }
    else if (p->file != NULL)
    {
      /* Defensive: ensure file_bytes is sane. */
      size_t bytes_to_read = PGSIZE;
      if (p->file_bytes > 0 && p->file_bytes <= PGSIZE)
        bytes_to_read = p->file_bytes;
      else
        {
          /* Log suspicious metadata once to help debugging. */
          printf("do_page_in: suspicious file_bytes=%"PRIu32" offset=%"PRIu32" for addr %p; using full-page read\n",
                 p->file_bytes, p->file_offset, p->addr);
        }

      off_t read_bytes = file_read_at (p->file, p->frame->base,
                                       bytes_to_read, p->file_offset);
      if (read_bytes < 0)
        read_bytes = 0;

      size_t zero_bytes = PGSIZE - (size_t) read_bytes;
      memset (p->frame->base + (size_t) read_bytes, 0, zero_bytes);

      if ((size_t) read_bytes != bytes_to_read)
        printf ("do_page_in: actually read %"PRId64" != requested %zu\n",
                (int64_t) read_bytes, bytes_to_read);
    }

  else
    {
      /* Provide all-zero page. */
      memset (p->frame->base, 0, PGSIZE);
    }

  return true;
}

/* Faults in the page containing FAULT_ADDR.
   Returns true if successful, false on failure. */
bool
page_in (void *fault_addr)
{
  struct page *p;
  bool success;

  /* Can't handle page faults without a hash table. */
  if (thread_current ()->pages == NULL)
    return false;

  p = page_for_addr (fault_addr);
  if (p == NULL)
    return false;

  frame_lock (p);
  if (p->frame == NULL)
    {
      if (!do_page_in (p))
        return false;
    }
  ASSERT (lock_held_by_current_thread (&p->frame->lock));

  /* Install frame into page table. */
  success = pagedir_set_page (thread_current ()->pagedir, p->addr,
                              p->frame->base, !p->read_only);

  /* Release frame. */
  frame_unlock (p->frame);

  return success;
}

/* Evicts page P.
   P must have a locked frame.
   Return true if successful, false on failure. */
bool
page_out (struct page *p)
{
  bool dirty;
  bool ok = false;

  ASSERT (p->frame != NULL);
  ASSERT (lock_held_by_current_thread (&p->frame->lock));

  /* Read dirty bit BEFORE clearing PTE (more reliable on some pagedir impls). */
  dirty = pagedir_is_dirty (p->thread->pagedir, (const void *) p->addr);

  /* Remove mapping so accesses fault while we write/swap. */
  pagedir_clear_page(p->thread->pagedir, (void *) p->addr);

  /* If page is clean and file-backed, no need to write it out. */
  if (!dirty)
    ok = true;

  /* Anonymous page (no file backing) -> must swap out. */
  if (p->file == NULL)
    {
      ok = swap_out(p);
    }
  else
    {
      /* File-backed. If dirty, try to write back to file; otherwise no-op. */
      if (dirty)
        {
          if (p->private)
            {
              /* Private mapping: changes should go to swap (copy-on-write style). */
              ok = swap_out(p);
            }
          else
            {
              off_t written = file_write_at(p->file, (const void *) p->frame->base,
                                            p->file_bytes, p->file_offset);
              if (written == (off_t) p->file_bytes)
                {
                  ok = true;
                }
              else
                {
                  
                 
                  ok = swap_out(p);
                }
            }
        }
    }

  if (ok)
    {
      /* Successfully saved page (either by not needing to, by writing to file,
         or by swapping out). Remove frame reference. */
      p->frame = NULL;
    }

  return ok;
}



/* Returns true if page P's data has been accessed recently,
   false otherwise.
   P must have a frame locked into memory. */
bool
page_accessed_recently (struct page *p)
{
  bool was_accessed;

  ASSERT (p->frame != NULL);
  ASSERT (lock_held_by_current_thread (&p->frame->lock));

  was_accessed = pagedir_is_accessed (p->thread->pagedir, p->addr);
  if (was_accessed)
    pagedir_set_accessed (p->thread->pagedir, p->addr, false);
  return was_accessed;
}

/* Adds a mapping for user virtual address VADDR to the page hash
   table.  Fails if VADDR is already mapped or if memory
   allocation fails. */
struct page *
page_allocate (void *vaddr, bool read_only)
{struct thread *t = thread_current ();
  struct page *p;
  
  /* Round down to page boundary */
  void *page_addr = pg_round_down (vaddr);
  
  /* Check if page already exists */
  p = page_for_addr (page_addr);
  if (p != NULL)
    return NULL; /* Already mapped */
  
  /* Allocate new page structure */
  p = malloc (sizeof (struct page));
  if (p == NULL)
    return NULL;
  
  /* Initialize page structure */
  p->addr = page_addr;
  p->read_only = read_only;
  p->thread = t;
  p->frame = NULL;
  p->sector = (block_sector_t) -1;  /* CRITICAL BUG FIX: Mark as not in swap */
  p->file = NULL;
  p->file_offset = 0;
  p->file_bytes = 0;
  p->private = false;
  
  /* Insert into hash table */
  if (hash_insert (t->pages, &p->hash_elem) != NULL)
    {
      /* Duplicate entry - shouldn't happen */
      free (p);
      return NULL;
    }
  
  return p;

}

/* Evicts the page containing address VADDR
   and removes it from the page table. */
void
page_deallocate (void *vaddr)
{
  /* Deallocate (remove) the page for given address.
   TODO: Implement this function.
   - Round addr down
   - Lookup page with page_for_address()
   - If found, remove from hash (inside lock) and free()
*/
 struct page *p;
  
  /* Round address down to page boundary */
  void *page_addr = pg_round_down (vaddr);
  
  /* Look up the page */
  p = page_for_addr (page_addr);
  if (p == NULL)
    return; /* Page doesn't exist */
  
  /* Lock the frame if it exists */
  frame_lock (p);
  
  /* Free the frame if allocated */
  if (p->frame != NULL)
    {
      frame_free (p->frame);
    }
  
  /* Remove from hash table */
  hash_delete (thread_current ()->pages, &p->hash_elem);
  
  /* Free the page structure */
  free (p);
}

/* Returns a hash value for the page that E refers to. */
unsigned
page_hash (const struct hash_elem *e, void *aux UNUSED)
{
  const struct page *p = hash_entry (e, struct page, hash_elem);
  return ((uintptr_t) p->addr) >> PGBITS;
}

/* Returns true if page A precedes page B. */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->addr < b->addr;
}

/* Tries to lock the page containing ADDR into physical memory.
   If WILL_WRITE is true, the page must be writeable;
   otherwise it may be read-only.
   Returns true if successful, false on failure. */
bool
page_lock (const void *addr, bool will_write)
{
  struct page *p = page_for_addr (addr);
  if (p == NULL || (p->read_only && will_write))
    return false;

  frame_lock (p);
  if (p->frame == NULL)
    return (do_page_in (p)
            && pagedir_set_page (thread_current ()->pagedir, p->addr,
                                 p->frame->base, !p->read_only));
  else
    return true;
}

/* Unlocks a page locked with page_lock(). */
void
page_unlock (const void *addr)
{
  struct page *p = page_for_addr (addr);
  ASSERT (p != NULL);
  frame_unlock (p->frame);
}