#include <filesys/file.h>
#include <string.h>
#include "userprog/syscall.h"
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
  return hash_int ((int)entry->upage);
}

static bool
spte_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct sup_page_table_entry *a_entry = hash_entry(a, struct sup_page_table_entry, h_elem);
  struct sup_page_table_entry *b_entry = hash_entry(b, struct sup_page_table_entry, h_elem);
  return a_entry->upage < b_entry->upage;
}

static void
spte_destroy_func(struct hash_elem *elem, void *aux UNUSED)
{
  struct sup_page_table_entry *entry = hash_entry(elem, struct sup_page_table_entry, h_elem);
  if (entry->on_frame) {
    free_frame (entry->kpage);
  } else if (entry->source == SWAP) {
    free_swap_slot (entry->swap_index);
  }
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
sup_page_table_get_entry (struct hash *sup_page_table, void *upage)
{
  struct sup_page_table_entry tmp_spte;
  tmp_spte.upage = upage;

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
sup_page_install_zero_page (void *upage)
{
  struct hash *spt = thread_current ()->spt;
  struct sup_page_table_entry *spte = malloc (sizeof (struct sup_page_table_entry));

  spte->upage = upage;
  spte->kpage = NULL;
  spte->on_frame = false;
  spte->writable = true;
  spte->dirty = false;
  spte->accessed = false;
  // TODO: Accessed
  spte->source = ALL_ZERO;

  struct hash_elem *prev_elem;
  prev_elem = hash_insert (spt, &spte->h_elem);
  if (prev_elem != NULL) {
    // TODO: Unexpected dup entry? Need validation?
    printf ("Duplicate entry when installing zero page\n");
    free (spte);
    return false;
  }
  return true;
}

/*
 * Save what to do later on spt (Installing page with source FILE_SYS)
 * Later, allocate user frame, read file, install page
 */
bool
sup_page_reserve_segment (void *upage, struct file * file, off_t offset, uint32_t page_read_bytes, uint32_t page_zero_bytes, bool writable)
{
  struct hash *spt = thread_current ()->spt;
  struct sup_page_table_entry *spte = malloc (sizeof (struct sup_page_table_entry));
  spte->upage = upage;
  spte->kpage = NULL;
  spte->on_frame = false;
  spte->dirty = false;
  spte->accessed = false;
  // TODO: Access time

  printf("Reserve segment %x %x\n", (int *)upage, (int *) offset);
  spte->source = FILE_SYS;
  spte->file = file;
  spte->file_offset = offset;
  spte->file_page_read_bytes = page_read_bytes;
  spte->file_page_zero_bytes = page_zero_bytes;
  spte->writable = writable;

  struct hash_elem *prev_elem;
  prev_elem = hash_insert (spt, &spte->h_elem);
  if (prev_elem == NULL) return true;
  // TODO: Unexpected dup entry? Need validation?
  printf ("Duplicate entry when reserving new segment from file\n");
  free (spte);
  return false;
}

static bool
load_from_filesys (struct sup_page_table_entry *spte, void *kpage)
{
  struct file *file = spte->file;
  uint32_t page_read_bytes = spte->file_page_read_bytes;
  uint32_t page_zero_bytes = spte->file_page_zero_bytes;
  off_t ofs = spte->file_offset;

  sema_down_filesys ();
  file_seek (file, ofs);

  if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
  {
    return false;
  }
  sema_up_filesys ();
  memset (kpage + page_read_bytes, 0, page_zero_bytes);
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
sup_page_load_page_and_pin (void *upage, bool pinned, bool create_new)
{
  struct thread *cur = thread_current ();
  uint32_t *pagedir = cur->pagedir;
  struct hash *spt = cur->spt;
  struct sup_page_table_entry *spte;
  void *kpage;
  bool writable;

  spte = sup_page_table_get_entry (spt, upage);

  if(spte == NULL) {
    if (create_new) {
      sup_page_install_zero_page(upage);
      return sup_page_load_page_and_pin (upage, pinned, false);
    }
    return false;
  }

  writable = spte->writable;
  if(spte->on_frame) {
    fte_update_pinned (spte->kpage, pinned);
//    printf ("Dup load request.\n");
    return true;
  }

  kpage = allocate_frame_and_pin (PAL_USER | PAL_ZERO, upage, pinned);

  if (kpage == NULL)
    return false;

  switch (spte->source)
  {
    case FILE_SYS:
      if (!load_from_filesys (spte, kpage))
      {
        printf("Load from filesys fail\n");
        free_frame (kpage);
        return false;
      }
      break;
    case SWAP:
      swap_in (spte->swap_index, kpage);
      break;
    case ALL_ZERO:
      // Nothing to do
      break;
    default:
      printf ("Unknown SPTE source %d\n", spte->source);
      return false;
  }

  if (!pagedir_set_page (pagedir, upage, kpage, writable))
  {
    free_frame (kpage);
    return false;
  }

  // Success!
  spte->on_frame = true;
  spte->kpage = kpage;
  fte_install_spte (kpage, spte);

  return true;
}

bool
sup_page_load_page (void *upage)
{
  return sup_page_load_page_and_pin (upage, false, false);
}

bool
sup_page_update_frame_pinned (void *upage, bool pinned)
{
  struct thread *cur = thread_current ();
  struct hash *spt = cur->spt;
  struct sup_page_table_entry *spte;

  spte = sup_page_table_get_entry (spt, upage);

  if(spte == NULL) {
    return false;
  }

  fte_update_pinned (spte->kpage, pinned);
  return true;
}

// TODO: Not sure about this function..
bool
sup_page_install_frame (struct hash *sup_page_table, void *upage, void *kpage)
{
  struct sup_page_table_entry *spte;

  spte = malloc (sizeof (struct sup_page_table_entry));
  spte->upage = upage;
  spte->kpage = kpage;
  spte->writable = true; // TODO: Hmm..

  spte->on_frame = true;
  spte->dirty = false;
  spte->accessed = false;

  struct hash_elem *prev_elem;

  prev_elem = hash_insert (sup_page_table, &spte->h_elem); // TODO: Per process access to each hash, need synchronization?

  if (prev_elem == NULL) {
    fte_install_spte (kpage, spte);
    return true;
  }
  else {
    free (spte);
    return false;
  }
}