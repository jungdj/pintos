#include <stdio.h>
#include <hash.h>
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

static struct lock frame_table_lock;
static struct hash frame_table;

static unsigned
frame_hash_func (const struct hash_elem *elem, void *aux UNUSED)
{
  struct frame_table_entry *frame_entry = hash_entry (elem, struct frame_table_entry, h_elem);
  return hash_bytes((void *) &frame_entry->frame, sizeof frame_entry->frame);
}
static bool
frame_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct frame_table_entry *a_entry = hash_entry (a, struct frame_table_entry, h_elem);
  struct frame_table_entry *b_entry = hash_entry (b, struct frame_table_entry, h_elem);
  return a_entry->frame < b_entry->frame;
}

/*
 * Initialize frame table
 */
void 
frame_init (void)
{
  lock_init (&frame_table_lock);
  hash_init (&frame_table, frame_hash_func, frame_less_func, NULL);
}

struct frame_table_entry*
get_frame_table_entry (void *kpage)
{
  struct frame_table_entry tmp_fte;
  tmp_fte.frame = (void *) vtop (kpage);

  struct hash_elem *elem = hash_find (&frame_table, &tmp_fte.h_elem);
  if (elem == NULL) return NULL;
  return hash_entry (elem, struct frame_table_entry, h_elem);
}

/* 
 * Make a new frame table entry for addr.
 */
void *
allocate_frame_and_pin (enum palloc_flags flags, void *upage, bool pinned)
{
  ASSERT (flags & PAL_USER)
  uint8_t *kpage;
  struct sup_page_table_entry *spte;
  struct frame_table_entry *fte;
  size_t swap_index;

  lock_acquire (&frame_table_lock);
  kpage = palloc_get_page (flags);
  // Allocation failed
  if (kpage == NULL) {
#ifdef VM_SWAP_H
    fte = select_victim_frame ();
    spte = fte->spte;
    pagedir_clear_page (fte->owner->pagedir, fte->upage);
    kpage = fte->kpage;
    // dirty check
    if (pagedir_is_dirty (fte->owner->pagedir, fte->upage) || spte->writable) {
      swap_index = swap_out (kpage);
      spte->source = SWAP;
      spte->swap_index = swap_index;
      spte->dirty = true;
    } else {
      spte->source = FILE_SYS;
    };
    spte->on_frame = false;
    free_frame_with_lock (kpage);
#else
    return NULL;
#endif
  }

  fte = malloc (sizeof (struct frame_table_entry));
  // TODO : Check validity ?
  fte->owner = thread_current ();
  fte->frame = (void *) vtop (kpage);
  // TODO: supplementary table ?
  fte->kpage = kpage; // TODO: vaddr?
  fte->upage = upage;
  fte->pinned = pinned;

  hash_insert (&frame_table, &fte->h_elem);
  lock_release (&frame_table_lock);

  return kpage;
}

void *
allocate_frame (enum palloc_flags flags, void *upage)
{
  return allocate_frame_and_pin (flags, upage, false);
}

void
fte_update_pinned (void *kpage, bool pinned)
{
  struct frame_table_entry *fte = get_frame_table_entry (kpage);
  fte->pinned = pinned;
}

void
fte_install_spte (void *kpage, struct sup_page_table_entry *spte)
{
  struct frame_table_entry *fte = get_frame_table_entry (kpage);
  fte->spte = spte;
}

void
free_frame (void *kpage)
{
  lock_acquire (&frame_table_lock);
  free_frame_with_lock (kpage);
  lock_release (&frame_table_lock);
}

/*
 * Free a frame table entry from kpage.
 * kpage must be kernel virtual address.
 * Must acquire frame_table_lock beforehand
 */
void
free_frame_with_lock (void *kpage)
{
  ASSERT (lock_held_by_current_thread(&frame_table_lock));
  struct frame_table_entry *fte;
  struct frame_table_entry *tmp_fte;

  // TODO: Check if kpage valid ?
  tmp_fte = (struct frame_table_entry *) malloc (sizeof (struct frame_table_entry));

  tmp_fte->frame = (void *) vtop (kpage);
  struct hash_elem *h_elem = hash_find (&frame_table, &tmp_fte->h_elem);
  free (tmp_fte);
  if (h_elem == NULL) {
    // TODO: Frame not found!? handle it
//    PANIC ("Can not find hash_elem\n");
    return;
  }

  fte = hash_entry (h_elem, struct frame_table_entry, h_elem);
  hash_delete (&frame_table, &fte->h_elem);
//  pagedir_clear_page (fte->owner->pagedir, fte->upage);
//  palloc_free_page (fte->kpage);
  free (fte);
}

void *
select_victim_frame (void)
{
  static size_t victim_index = 0;
  size_t n;
  struct hash_iterator it;
  size_t i;

  n = hash_size (&frame_table);

  hash_first (&it, &frame_table);
  for(i = 0; i <= victim_index % n; ++i)
    hash_next (&it);

  do {
    struct frame_table_entry *fte = hash_entry (hash_cur (&it), struct frame_table_entry, h_elem);
    victim_index = (victim_index + 1) % n;
    if(fte->pinned) continue;
    if(!pagedir_is_accessed (fte->owner->pagedir, fte->upage)) {
      return fte;
    }
    pagedir_set_accessed (fte->owner->pagedir, fte->upage, false);
  } while (hash_next (&it));

  hash_first (&it, &frame_table);
  hash_next (&it);

  do {
    struct frame_table_entry *fte = hash_entry(hash_cur (&it), struct frame_table_entry, h_elem);
    victim_index = (victim_index + 1) % n;
        if(fte->pinned) continue;
    if(!pagedir_is_accessed (fte->owner->pagedir, fte->upage)) {
      return fte;
    }
    pagedir_set_accessed (fte->owner->pagedir, fte->upage, false);
  } while (hash_next(&it));

  PANIC("Can not reach here. Select_victim_frame");
}