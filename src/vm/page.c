#include "vm/page.h"
#include "vm/frame.h"
#include "threads/malloc.h"

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