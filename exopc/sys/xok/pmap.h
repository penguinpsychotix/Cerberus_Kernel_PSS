
/*
 * Copyright (C) 1997 Massachusetts Institute of Technology 
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose and without fee or royalty is
 * hereby granted, provided that the full text of this NOTICE appears on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. BY WAY OF EXAMPLE,
 * BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
 * THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
 * THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. COPYRIGHT
 * HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE OR
 * DOCUMENTATION.
 *
 * The name and trademarks of copyright holders may NOT be used in
 * advertising or publicity pertaining to the software without specific,
 * written prior permission. Title to copyright in this software and any
 * associated documentation will at all times remain with copyright
 * holders. See the file AUTHORS which should have accompanied this software
 * for a list of all copyright holders.
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by the copyright
 * holders listed in the AUTHORS file. The rest of this file is covered by
 * the copyright notices, if any, listed below.
 */

#ifndef _XOK_PMAP_H_
#define _XOK_PMAP_H_

/*
 * This file contains specifications of data structures and functions
 * dealing with physical pages. 
 */

#include <xok_include/assert.h>
#include <xok/capability.h>
#include <xok/queue.h>
#include <xok/vpage.h>
#include <xok/bc.h>
#include <xok/mplock.h>
#include <xok/pmap_decl.h>

struct Env;
struct EnvPD;

struct Pp_state {	          
  unsigned char ps_writers: 4;
  unsigned char ps_readers: 4;
};

#ifdef KERNEL

static inline struct Pp_state* Pp_state_alloc()
{
  return (struct Pp_state*) malloc(sizeof(struct Pp_state));
}

static inline int 
ppcompat_state (u_int pp_readers, u_int pp_writers, const struct Pp_state *s) 
{
  if (s->ps_readers == PP_ACCESS_ALL && s->ps_writers == PP_ACCESS_ALL)
    return 1;
  if (pp_readers > s->ps_readers || pp_writers > s->ps_writers)
    return 0;
  return 1;
}

#endif

/* Ppage list bookkepping */

/* the following defines two Ppage lists: a linked list and a queue */
LIST_HEAD(Ppage_list, Ppage);
typedef LIST_ENTRY(Ppage) Ppage_LIST_entry_t;

TAILQ_HEAD(free_buf_list, Ppage);
typedef TAILQ_ENTRY(Ppage) Ppage_TAILQ_entry_t;

struct Ppage {
  union {
    Ppage_LIST_entry_t ppu_link;   	/* free list link */
    Ppage_TAILQ_entry_t ppu_buf_link; 	/* free but filled buf link */
    struct {
      Vpage_list_t ppuv_pl;       	/* vpage list */
      u_short ppuv_rcbn;          	/* refcount/blocksize # */
      u_short ppuv_pcfc;          	/* pagecount/free count */
    } ppl_v;
  } pp_u;
#define pp_link pp_u.ppu_link
#define pp_buf_link pp_u.ppu_buf_link
#define pp_vpl pp_u.ppl_v.ppuv_pl
#define pp_refcnt pp_u.ppl_v.ppuv_rcbn
#define pp_bs pp_u.ppl_v.ppuv_rcbn
#define pp_pcfc pp_u.ppl_v.ppuv_pcfc
  unsigned char pp_status;        /* status of ppage - see pmap.h */
  struct Pp_state pp_state;       /* access limits to each page */
  cap pp_acl[PP_ACL_LEN];
  u_short pp_readers, pp_writers; /* number of each type of mapping */
  struct bc_entry *pp_buf;	  /* buf struct if page is in buffer cache */
  int pp_pinned;
  struct kspinlock pp_klock; /* kernel lock for this Ppage struct */
};

#ifdef KERNEL

static inline void 
Ppagelist_head_insert(struct Ppage_list* plist, struct Ppage* pp)
{
  LIST_INSERT_HEAD(plist, pp, pp_link);
}

static inline void 
Ppagelist_head_remove(struct Ppage* pp)
{
  LIST_REMOVE(pp, pp_link);
}

static inline void 
freebuflist_tail_insert(struct free_buf_list* flist, struct Ppage* pp)
{
  TAILQ_INSERT_TAIL(flist, pp, pp_buf_link);
}

static inline void 
freebuflist_tail_remove(struct free_buf_list* flist, struct Ppage* pp)
{
  TAILQ_REMOVE(flist, pp, pp_buf_link);
}

static inline void 
Ppage_pp_klock_acquire(struct Ppage *pp)
{
  extern u_quad_t lock_pp_counter;
  lock_pp_counter++;
  MP_SPINLOCK_GET(&pp->pp_klock);
}

static inline int 
Ppage_pp_klock_try(struct Ppage *pp)
{
  extern u_quad_t lock_pp_counter;
  lock_pp_counter++;
#ifdef __SMP__
  if (smp_commenced)
    return kspinlock_try(&pp->pp_klock);
  else
#endif
    return 0;
}

static inline void 
Ppage_pp_klock_release(struct Ppage *pp)
{
  MP_SPINLOCK_RELEASE(&pp->pp_klock);
}

static inline void 
Ppage_pp_pcfc_atomic_inc(struct Ppage *pp)
{
  asm volatile("lock\n\tincl %0\n" : "=m" (pp->pp_pcfc) : "0" (pp->pp_pcfc));
}

static inline void 
Ppage_pp_pcfc_atomic_dec(struct Ppage *pp)
{
  asm volatile("lock\n\tdecl %0\n" : "=m" (pp->pp_pcfc) : "0" (pp->pp_pcfc));
}

static inline struct kspinlock* 
Ppage_pp_klock_ptr(struct Ppage * pp)
{
  return &(pp->pp_klock);
}

#endif /* KERNEL */


#ifndef KERNEL  /* libOS - __ppages */

/* this is the array of all physical pages */
extern struct Ppage __ppages[];

#else           /* KERNEL - ppages */

extern struct Ppage *ppages;
extern u_long nppage;

#endif /* KERNEL */


#ifdef KERNEL

/* initializes all the Ppage structures */
void ppage_init (void);

/* allocates n contiguous physical page frames */
int  ppage_alloc_n (u_char, struct Ppage **, int);

/* allocates a physical page */
int  ppage_alloc (u_char, struct Ppage **, int userreq);

/* guards physical page with capability */
void ppage_setcap (struct Ppage *pp, cap *c);

/* moves a page to the proper free list depending on if it contains a buffer */
void ppage_free (struct Ppage *);

/* maps a page in a specified environment */
int  ppage_insert (struct Env *, struct Ppage *, u_int, u_int);

/* unmaps a page from an environment. returns 0 if successful. if returns -1,
 * environment can no longer be run because env_u is removed ... */
int ppage_remove (struct Env *const e, u_int va);

/* moves a page from free_bufs to free_list - the page is removed from the
 * buffer cache */
void ppage_free_bufs_2_free_list (struct Ppage *pp);

/* removes a page from the free_bufs list and allow the page to be mapped
 * again. keeps the buffer in the page */
void ppage_reuse_buffer (struct Ppage *pp);

/* specifically moves a page off the free_bufs list and initialize it in
 * preparation of being mapped. DANGER: this will happily reuse dirty buffers
 * which might not be what you want */
void ppage_reclaim_buffer (struct Ppage *pp, int type);

/* like ppage_alloc, but go ahead and inserts the allocated page into the
 * buffer cache and put it on free_bufs. Good for reading data into the buffer
 * cache yet still allowing the page to be reclaimed if memory gets low */
struct bc_entry *
ppage_get_reclaimable_buffer (u32 dev, u_quad_t block, int bc_state, 
                              int *error, int userreq);

/* specifically moves a page off the free page list and initialize it in
 * preparation of being mapped */
void ppage_reclaim_page (struct Ppage *pp, int type);
   
/* removes a page from either free_list or free_bufs, depending upon which
 * it's on and then init it */
void ppage_reclaim (struct Ppage *pp, int type);

/* allows a physical page where some i/o device lies to be mapped.  */
void ppage_grant_iomem (struct Ppage *pp);

/* allocates a page directory entry if one is needed for the va, insert the
 * new entry in all the page directories for the environment given */
int  pde_check_n_alloc (struct EnvPD* envpd, u_int va);

/* same as pde_check_n_alloc, but only insert to the specified pdir */
int  pde_check_n_alloc_one (Pde *const, u_int va);

/* create a new page directory by copying all PDEs from an existing directory.
 * Does not hierarchically copy PTEs. Returns the page containing the
 * directory. */
struct Ppage * pd_duplicate(Pde *const);

/* inserts a range of pages into the page table of the environment specified.
 * The num_completed field allows the function to be interrupted by a page
 * fault or to return an error and then restart at the proper location without
 * redoing what's already been done */
int  
pmap_insert_range (u_int sn, u_int k, Pte *ptes, u_int num, u_int va, 
                   u_int *num_completed, u_int ke, int envid, int userreq);

/* invalidates the TLB if envid is current environment's id */
void tlb_invalidate (u_int va, u_int addrspace_id);

/* marks a page pinned. increment the system pinned page count if this is the
 * first time the page is pinned */
void ppage_pin (struct Ppage *pp);

/* unpins a page. decrement the system pinned page count if necessary */
void ppage_unpin (struct Ppage *pp);

/* returns result of performing acl access check on ppage's cap */
int ppage_acl_check (struct Ppage *pp, cap *c, int len, u_char perm);

/* zero out ppage acl */
void ppage_acl_zero (struct Ppage *pp);

/* translates from a Ppage structure to a physical address */
#define pp2pa(ppage_p) ((u_long) (pp2ppn (ppage_p) << PGSHIFT))

/* translates from a Ppage structure to a virtual address */
#define pp2va(ppage_p) ((void *) (KERNBASE + pp2pa (ppage_p)))

static inline int 
pp2ppn(struct Ppage* pp)
{
  return ((pp) - ppages);
}

static inline struct Ppage* 
pa2pp(u_long kva)
{
  u_long ppn = (u_long) (kva) >> PGSHIFT;
  if (ppn >= nppage)
    panic (__FUNCTION__ ":%d: pa2pp called with invalid pa",
	   __LINE__);
  return &ppages[ppn]; 
}

static inline struct Ppage* 
kva2pp(u_long kva)
{
  u_long ppn = ((u_long) (kva) - KERNBASE) >> PGSHIFT;
  if (ppn >= nppage)
    panic (__FUNCTION__ ":%d: kva2pp called with invalid kva",
       __LINE__);
  return &ppages[ppn];
}

static inline Pte *
pt_get_ptep (const Pde * pdir, u_int va) 
{
  Pde pde;
  Pte *pt;

  pde = pdir[PDENO (va)];
  pt = ptov (pde & ~PGMASK);
  return (&pt[PTENO (va)]);
}

static inline int
ppage_is_reclaimable(const struct Ppage* pp)
{
  return ((pp->pp_status == PP_FREE) && (pp->pp_pinned == 0) && 
      ((pp->pp_buf == NULL) || (bc_is_reclaimable(pp->pp_buf))));
}

#endif /* KERNEL */
#endif /* _XOK_PMAP_H_ */

