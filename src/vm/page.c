#include <filesys/file.h>
#include <string.h>
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "filesys/off_t.h"
#include "vm/page.h"
#include "vm/frame.h"

static unsigned
spte_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct sup_page_table_entry *entry = hash_entry (elem, struct sup_page_table_entry, h_elem);
  return hash_int ((int)entry->user_vaddr);
}

static bool
spte_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct sup_page_table_entry *a_entry = hash_entry(a, struct sup_page_table_entry, h_elem);
  struct sup_page_table_entry *b_entry = hash_entry(b, struct sup_page_table_entry, h_elem);
  return a_entry->user_vaddr < b_entry->user_vaddr;
}

static void
spte_destroy_func(struct hash_elem *elem, void *aux UNUSED)
{
  struct sup_page_table_entry *entry = hash_entry(elem, struct sup_page_table_entry, h_elem);
  free (entry);
}

struct hash *
sup_page_create (void)
{
  struct hash *sup_page_table = (struct hash*) malloc(sizeof (struct hash));
  hash_init (sup_page_table, spte_hash_func, spte_less_func, NULL);
  return sup_page_table;
}

void
sup_page_destroy (struct hash *sup_page_table)
{
  ASSERT (sup_page_table != NULL)
  hash_destroy (sup_page_table, spte_destroy_func);
  free (sup_page_table);
}

struct sup_page_table_entry*
sup_page_table_get_entry (struct hash *sup_page_table, void *vaddr)
{
  struct sup_page_table_entry tmp_spte;
  tmp_spte.user_vaddr = vaddr;

  struct hash_elem *elem = hash_find (sup_page_table, &tmp_spte.h_elem);
  if (elem == NULL) return NULL;
  return hash_entry (elem, struct sup_page_table_entry, h_elem);
}

bool
sup_page_table_has_entry (struct hash *sup_page_table, void *vaddr)
{
  struct sup_page_table_entry *spte = sup_page_table_get_entry (sup_page_table, vaddr);
  if(spte == NULL) return false;
  return true;
}

bool
sup_page_install_zero_page (void *vaddr)
{
  struct hash *spt = thread_current ()->sup_page_table;
  struct sup_page_table_entry *spte = malloc (sizeof (struct sup_page_table_entry));
  spte->user_vaddr = vaddr;
  spte->source = ALL_ZERO;

  struct hash_elem *prev_elem;
  prev_elem = hash_insert (spt, &spte->h_elem);
  if (prev_elem != NULL) {
    // TODO: Unexpected dup entry? Need validation?
    printf ("Duplicate entry when installing zero page\n");
    free (spte);
    return false;
  }
  return sup_page_load_page (vaddr);
}

/*
 * Save what to do later on spt (Installing page with source FILE_SYS)
 * Later, allocate user frame, read file, install page
 */
bool
sup_page_reserve_segment (void *vaddr, struct file * file, off_t offset, uint32_t page_read_bytes, uint32_t page_zero_bytes, bool writable)
{
  struct hash *spt = thread_current ()->sup_page_table;
  struct sup_page_table_entry *spte = malloc (sizeof (struct sup_page_table_entry));
  spte->user_vaddr = vaddr;

  spte->source = FILE_SYS;
  spte->file = file;
  spte->file_offset = offset;
  spte->file_page_read_bytes = page_read_bytes;
  spte->file_page_zero_bytes = page_zero_bytes;
  spte->file_writable = writable;

  struct hash_elem *prev_elem;
  prev_elem = hash_insert (spt, &spte->h_elem);
  // TODO: Lazy load frames. -> Done
  if (prev_elem == NULL) return true;
  // TODO: Unexpected dup entry? Need validation?
  printf ("Duplicate entry when reserving new segment from file\n");
  free (spte);
  return false;
}

static bool
load_from_filesys (struct sup_page_table_entry *spte, void *frame)
{
  struct file *file = spte->file;
  uint32_t page_read_bytes = spte->file_page_read_bytes;
  uint32_t page_zero_bytes = spte->file_page_zero_bytes;
  off_t ofs = spte->file_offset;

  file_seek (file, ofs);

  if (file_read (file, frame, page_read_bytes) != (int) page_read_bytes)
  {
    return false;
  }
  memset (frame + page_read_bytes, 0, page_zero_bytes);
  return true;
}

/*
 * * Load Page
   * Get info whether
   * 0. Already loaded
   * 1. Has info of
   *  1. From filesys
   *  2. From swap slot
   *  3. All zero
   * 2. Load
 */
bool
sup_page_load_page (void *vaddr)
{
  struct thread *cur = thread_current ();
  uint32_t *pagedir = cur->pagedir;
  struct hash *spt = cur->sup_page_table;
  struct sup_page_table_entry *spte;
  void *frame;
  bool writable = true;

  spte = sup_page_table_get_entry (spt, vaddr);

  if(spte == NULL) {
    return false;
  }

  if(spte->on_frame) {
    printf ("Dup load request.\n");
    return false; // Duplicate request
  }

  frame = allocate_frame (PAL_USER | PAL_ZERO, NULL);

  if (frame == NULL)
    return false;

  switch (spte->source)
  {
    case FILE_SYS:
      // TODO: Lazy load file segment -> Done
      writable = spte->file_writable;
      if (!load_from_filesys (spte, frame))
      {
        free_frame (frame);
        return false;
      }
      break;
    case SWAP:
      // TODO: Swap in
      break;
    case ALL_ZERO:
      // Nothing to do
      break;
    default:
      printf ("Unknown SPTE source %d\n", spte->source);
      return false;
  }

  if (!pagedir_set_page (pagedir, vaddr, frame, writable))
  {
    free_frame (frame);
    return false;
  }

  // Success!
  spte->on_frame = true;
  spte->frame = frame;

  return true;
}

// TODO: Not sure about this function..
bool
sup_page_install_frame (struct hash *sup_page_table, void *upage, void *kpage)
{
  struct sup_page_table_entry *spte = malloc (sizeof (struct sup_page_table_entry));
  spte->user_vaddr = upage;
  spte->frame = kpage;
  struct hash_elem *prev_elem;
  prev_elem = hash_insert (sup_page_table, &spte->h_elem); // TODO: Per process access to each hash, need synchronization?
  if (prev_elem == NULL) {
    return true;
  }
  else {
    free (spte);
    return false;
  }
}