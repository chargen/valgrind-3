/*
  This file is part of drd, a data race detector.

  Copyright (C) 2006-2008 Bart Van Assche
  bart.vanassche@gmail.com

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
  02111-1307, USA.

  The GNU General Public License is contained in the file COPYING.
*/


#include "drd_basics.h"           /* DRD_() */
#include "drd_bitmap.h"
#include "drd_error.h"
#include "drd_suppression.h"
#include "pub_drd_bitmap.h"
#include "pub_tool_basics.h"      /* Addr, SizeT */
#include "pub_tool_debuginfo.h"   /* VG_(get_objname)() */
#include "pub_tool_libcassert.h"  /* tl_assert() */
#include "pub_tool_libcbase.h"    /* VG_(memset) */
#include "pub_tool_libcprint.h"   /* VG_(printf) */
#include "pub_tool_machine.h"     /* VG_(get_IP)() */
#include "pub_tool_mallocfree.h"  /* VG_(malloc), VG_(free) */


/* Forward declarations. */

struct bitmap2;


/* Local function declarations. */

static void bm2_merge(struct bitmap2* const bm2l,
                      const struct bitmap2* const bm2r);


/* Local variables. */

static ULong s_bitmap_creation_count;
static ULong s_bitmap_merge_count;
static ULong s_bitmap2_merge_count;


/* Function definitions. */

struct bitmap* DRD_(bm_new)()
{
  return bm_new_cb(0);
}

/** Allocate and initialize a new bitmap structure.
 *
 *  @param compute_bitmap2 Callback function for computing the actual bitmap
 *                         data.
 */
struct bitmap* bm_new_cb(void (*compute_bitmap2)(UWord, struct bitmap2*))
{
  unsigned i;
  struct bitmap* bm;

  /* If this assert fails, fix the definition of BITS_PER_BITS_PER_UWORD */
  /* in drd_bitmap.h.                                                    */
  tl_assert((1 << BITS_PER_BITS_PER_UWORD) == BITS_PER_UWORD);

  bm = VG_(malloc)("drd.bitmap.bn.1", sizeof(*bm));
  tl_assert(bm);
  /* Cache initialization. a1 is initialized with a value that never can
   * match any valid address: the upper (ADDR_LSB_BITS + ADDR_IGNORED_BITS)
   * bits of a1 are always zero for a valid cache entry.
   */
  for (i = 0; i < N_CACHE_ELEM; i++)
  {
    bm->cache[i].a1  = ~(UWord)1;
    bm->cache[i].bm2 = 0;
  }
  bm->oset = VG_(OSetGen_Create)(0, 0, VG_(malloc), "drd.bitmap.bn.2",
                                       VG_(free));
  bm->compute_bitmap2 = compute_bitmap2;

  s_bitmap_creation_count++;

  return bm;
}

void DRD_(bm_delete)(struct bitmap* const bm)
{
  tl_assert(bm);

  VG_(OSetGen_Destroy)(bm->oset);
  VG_(free)(bm);
}

/**
 * Record an access of type access_type at addresses a .. a + size - 1 in
 * bitmap bm.
 *
 * @note The current implementation of bm_access_range does not work for the
 * highest addresses in the address range. At least on Linux this is
 * not a problem since the upper part of the address space is reserved
 * for the kernel.
 */
void DRD_(bm_access_range)(struct bitmap* const bm,
                           const Addr a1, const Addr a2,
                           const BmAccessTypeT access_type)
{
  Addr b, b_next;

  tl_assert(bm);
  tl_assert(a1 < a2);
  tl_assert(a2 < first_address_with_higher_msb(a2));

  for (b = a1; b < a2; b = b_next)
  {
    Addr b_start;
    Addr b_end;
    struct bitmap2* bm2;
    UWord b0 = address_lsb(b);
    const UWord b1 = address_msb(b);

    b_next = first_address_with_higher_msb(b);
    if (b_next > a2)
    {
      b_next = a2;
    }

    bm2 = bm2_lookup_or_insert_exclusive(bm, b1);
    tl_assert(bm2);

    if (make_address(bm2->addr, 0) < a1)
      b_start = a1;
    else
      if (make_address(bm2->addr, 0) < a2)
        b_start = make_address(bm2->addr, 0);
      else
        break;
    tl_assert(a1 <= b_start && b_start <= a2);

    if (first_address_with_higher_msb(make_address(bm2->addr, 0)) < a2)
      b_end = first_address_with_higher_msb(make_address(bm2->addr, 0));
    else
      b_end = a2;
    tl_assert(a1 <= b_end && b_end <= a2);
    tl_assert(b_start < b_end);
    tl_assert(address_lsb(b_start) <= address_lsb(b_end - 1));
      
    if (access_type == eLoad)
    {
      for (b0 = address_lsb(b_start); b0 <= address_lsb(b_end - 1); b0++)
      {
        bm0_set(bm2->bm1.bm0_r, b0);
      }
    }
    else
    {
      for (b0 = address_lsb(b_start); b0 <= address_lsb(b_end - 1); b0++)
      {
        bm0_set(bm2->bm1.bm0_w, b0);
      }
    }
  }
}

void DRD_(bm_access_range_load)(struct bitmap* const bm,
                                const Addr a1, const Addr a2)
{
  DRD_(bm_access_range)(bm, a1, a2, eLoad);
}

void DRD_(bm_access_load_1)(struct bitmap* const bm, const Addr a1)
{
  bm_access_aligned_load(bm, a1, 1);
}

void DRD_(bm_access_load_2)(struct bitmap* const bm, const Addr a1)
{
  if ((a1 & 1) == 0)
    bm_access_aligned_load(bm, a1, 2);
  else
    DRD_(bm_access_range)(bm, a1, a1 + 2, eLoad);
}

void DRD_(bm_access_load_4)(struct bitmap* const bm, const Addr a1)
{
  if ((a1 & 3) == 0)
    bm_access_aligned_load(bm, a1, 4);
  else
    DRD_(bm_access_range)(bm, a1, a1 + 4, eLoad);
}

void DRD_(bm_access_load_8)(struct bitmap* const bm, const Addr a1)
{
  if ((a1 & 7) == 0)
    bm_access_aligned_load(bm, a1, 8);
  else if ((a1 & 3) == 0)
  {
    bm_access_aligned_load(bm, a1 + 0, 4);
    bm_access_aligned_load(bm, a1 + 4, 4);
  }
  else
    DRD_(bm_access_range)(bm, a1, a1 + 8, eLoad);
}

void DRD_(bm_access_range_store)(struct bitmap* const bm,
                                 const Addr a1, const Addr a2)
{
  DRD_(bm_access_range)(bm, a1, a2, eStore);
}

void DRD_(bm_access_store_1)(struct bitmap* const bm, const Addr a1)
{
  bm_access_aligned_store(bm, a1, 1);
}

void DRD_(bm_access_store_2)(struct bitmap* const bm, const Addr a1)
{
  if ((a1 & 1) == 0)
    bm_access_aligned_store(bm, a1, 2);
  else
    DRD_(bm_access_range)(bm, a1, a1 + 2, eStore);
}

void DRD_(bm_access_store_4)(struct bitmap* const bm, const Addr a1)
{
  if ((a1 & 3) == 0)
    bm_access_aligned_store(bm, a1, 4);
  else
    DRD_(bm_access_range)(bm, a1, a1 + 4, eStore);
}

void DRD_(bm_access_store_8)(struct bitmap* const bm, const Addr a1)
{
  if ((a1 & 7) == 0)
    bm_access_aligned_store(bm, a1, 8);
  else if ((a1 & 3) == 0)
  {
    bm_access_aligned_store(bm, a1 + 0, 4);
    bm_access_aligned_store(bm, a1 + 4, 4);
  }
  else
    DRD_(bm_access_range)(bm, a1, a1 + 8, eStore);
}

Bool DRD_(bm_has)(struct bitmap* const bm, const Addr a1, const Addr a2,
                  const BmAccessTypeT access_type)
{
  Addr b;
  for (b = a1; b < a2; b++)
  {
    if (! DRD_(bm_has_1)(bm, b, access_type))
    {
      return False;
    }
  }
  return True;
}

Bool
DRD_(bm_has_any_load)(struct bitmap* const bm, const Addr a1, const Addr a2)
{
  Addr b, b_next;

  tl_assert(bm);

  for (b = a1; b < a2; b = b_next)
  {
    const struct bitmap2* bm2 = bm2_lookup(bm, address_msb(b));

    b_next = first_address_with_higher_msb(b);
    if (b_next > a2)
    {
      b_next = a2;
    }

    if (bm2)
    {
      Addr b_start;
      Addr b_end;
      UWord b0;
      const struct bitmap1* const p1 = &bm2->bm1;

      if (make_address(bm2->addr, 0) < a1)
        b_start = a1;
      else
        if (make_address(bm2->addr, 0) < a2)
          b_start = make_address(bm2->addr, 0);
        else
          break;
      tl_assert(a1 <= b_start && b_start <= a2);

      if (first_address_with_higher_msb(make_address(bm2->addr, 0)) < a2)
        b_end = first_address_with_higher_msb(make_address(bm2->addr, 0));
      else
        b_end = a2;
      tl_assert(a1 <= b_end && b_end <= a2);
      tl_assert(b_start < b_end);
      tl_assert(address_lsb(b_start) <= address_lsb(b_end - 1));
      
      for (b0 = address_lsb(b_start); b0 <= address_lsb(b_end - 1); b0++)
      {
        if (bm0_is_set(p1->bm0_r, b0))
        {
          return True;
        }
      }
    }
  }
  return 0;
}

Bool DRD_(bm_has_any_store)(struct bitmap* const bm,
                            const Addr a1, const Addr a2)
{
  Addr b, b_next;

  tl_assert(bm);

  for (b = a1; b < a2; b = b_next)
  {
    const struct bitmap2* bm2 = bm2_lookup(bm, address_msb(b));

    b_next = first_address_with_higher_msb(b);
    if (b_next > a2)
    {
      b_next = a2;
    }

    if (bm2)
    {
      Addr b_start;
      Addr b_end;
      UWord b0;
      const struct bitmap1* const p1 = &bm2->bm1;

      if (make_address(bm2->addr, 0) < a1)
        b_start = a1;
      else
        if (make_address(bm2->addr, 0) < a2)
          b_start = make_address(bm2->addr, 0);
        else
          break;
      tl_assert(a1 <= b_start && b_start <= a2);

      if (first_address_with_higher_msb(make_address(bm2->addr, 0)) < a2)
        b_end = first_address_with_higher_msb(make_address(bm2->addr, 0));
      else
        b_end = a2;
      tl_assert(a1 <= b_end && b_end <= a2);
      tl_assert(b_start < b_end);
      tl_assert(address_lsb(b_start) <= address_lsb(b_end - 1));
      
      for (b0 = address_lsb(b_start); b0 <= address_lsb(b_end - 1); b0++)
      {
        if (bm0_is_set(p1->bm0_w, b0))
        {
          return True;
        }
      }
    }
  }
  return 0;
}

/* Return True if there is a read access, write access or both   */
/* to any of the addresses in the range [ a1, a2 [ in bitmap bm. */
Bool DRD_(bm_has_any_access)(struct bitmap* const bm,
                             const Addr a1, const Addr a2)
{
  Addr b, b_next;

  tl_assert(bm);

  for (b = a1; b < a2; b = b_next)
  {
    const struct bitmap2* bm2 = bm2_lookup(bm, address_msb(b));

    b_next = first_address_with_higher_msb(b);
    if (b_next > a2)
    {
      b_next = a2;
    }

    if (bm2)
    {
      Addr b_start;
      Addr b_end;
      UWord b0;
      const struct bitmap1* const p1 = &bm2->bm1;

      if (make_address(bm2->addr, 0) < a1)
        b_start = a1;
      else
        if (make_address(bm2->addr, 0) < a2)
          b_start = make_address(bm2->addr, 0);
        else
          break;
      tl_assert(a1 <= b_start && b_start <= a2);

      if (first_address_with_higher_msb(make_address(bm2->addr, 0)) < a2)
        b_end = first_address_with_higher_msb(make_address(bm2->addr, 0));
      else
        b_end = a2;
      tl_assert(a1 <= b_end && b_end <= a2);
      tl_assert(b_start < b_end);
      tl_assert(address_lsb(b_start) <= address_lsb(b_end - 1));
      
      for (b0 = address_lsb(b_start); b0 <= address_lsb(b_end - 1); b0++)
      {
        if (bm0_is_set(p1->bm0_r, b0) | bm0_is_set(p1->bm0_w, b0))
        {
          return True;
        }
      }
    }
  }
  return False;
}

/**
 * Report whether an access of type access_type at address a is recorded in
 * bitmap bm.
 */
Bool DRD_(bm_has_1)(struct bitmap* const bm,
                    const Addr a, const BmAccessTypeT access_type)
{
  const struct bitmap2* p2;
  const struct bitmap1* p1;
  const UWord* p0;
  const UWord a0 = address_lsb(a);

  tl_assert(bm);

  p2 = bm2_lookup(bm, address_msb(a));
  if (p2)
  {
    p1 = &p2->bm1;
    p0 = (access_type == eLoad) ? p1->bm0_r : p1->bm0_w;
    return bm0_is_set(p0, a0) ? True : False;
  }
  return False;
}

void DRD_(bm_clear)(struct bitmap* const bm, const Addr a1, const Addr a2)
{
  Addr b, b_next;

  tl_assert(bm);
  tl_assert(a1);
  tl_assert(a1 <= a2);

  for (b = a1; b < a2; b = b_next)
  {
    struct bitmap2* const p2 = bm2_lookup_exclusive(bm, address_msb(b));

    b_next = first_address_with_higher_msb(b);
    if (b_next > a2)
    {
      b_next = a2;
    }

    if (p2)
    {
      Addr c = b;
      /* If the first address in the bitmap that must be cleared does not */
      /* start on an UWord boundary, start clearing the first addresses.  */
      if (uword_lsb(address_lsb(c)))
      {
        Addr c_next = first_address_with_higher_uword_msb(c);
        if (c_next > b_next)
          c_next = b_next;
        bm0_clear_range(p2->bm1.bm0_r, address_lsb(c),
                        SCALED_SIZE(c_next - c));
        bm0_clear_range(p2->bm1.bm0_w, address_lsb(c),
                        SCALED_SIZE(c_next - c));
        c = c_next;
      }
      /* If some UWords have to be cleared entirely, do this now. */
      if (uword_lsb(address_lsb(c)) == 0)
      {
        const Addr c_next = first_address_with_same_uword_lsb(b_next);
        tl_assert(uword_lsb(address_lsb(c)) == 0);
        tl_assert(uword_lsb(address_lsb(c_next)) == 0);
        tl_assert(c_next <= b_next);
        if (b_next < a2)
          tl_assert(c <= c_next);
        if (c_next > c)
        {
          const UWord idx = uword_msb(address_lsb(c));
          VG_(memset)(&p2->bm1.bm0_r[idx], 0, SCALED_SIZE((c_next - c) / 8));
          VG_(memset)(&p2->bm1.bm0_w[idx], 0, SCALED_SIZE((c_next - c) / 8));
          c = c_next;
        }
      }
      /* If the last address in the bitmap that must be cleared does not */
      /* fall on an UWord boundary, clear the last addresses.            */
      /* tl_assert(c <= b_next); */
      bm0_clear_range(p2->bm1.bm0_r, address_lsb(c),
                      SCALED_SIZE(b_next - c));
      bm0_clear_range(p2->bm1.bm0_w, address_lsb(c),
                      SCALED_SIZE(b_next - c));
    }
  }
}

/**
 * Clear all references to loads in bitmap bm starting at address a1 and
 * up to but not including address a2.
 */
void DRD_(bm_clear_load)(struct bitmap* const bm, const Addr a1, const Addr a2)
{
  Addr a;

  for (a = a1; a < a2; a++)
  {
    struct bitmap2* const p2 = bm2_lookup_exclusive(bm, address_msb(a));
    if (p2)
    {
      bm0_clear(p2->bm1.bm0_r, address_lsb(a));
    }
  }
}

/**
 * Clear all references to stores in bitmap bm starting at address a1 and
 * up to but not including address a2.
 */
void DRD_(bm_clear_store)(struct bitmap* const bm,
                          const Addr a1, const Addr a2)
{
  Addr a;

  for (a = a1; a < a2; a++)
  {
    struct bitmap2* const p2 = bm2_lookup_exclusive(bm, address_msb(a));
    if (p2)
    {
      bm0_clear(p2->bm1.bm0_w, address_lsb(a));
    }
  }
}

/**
 * Clear bitmap bm starting at address a1 and up to but not including address
 * a2. Return True if and only if any of the addresses was set before
 * clearing.
 */
Bool DRD_(bm_test_and_clear)(struct bitmap* const bm,
                             const Addr a1, const Addr a2)
{
  Bool result;

  result = DRD_(bm_has_any_access)(bm, a1, a2) != 0;
  DRD_(bm_clear)(bm, a1, a2);
  return result;
}

Bool DRD_(bm_has_conflict_with)(struct bitmap* const bm,
                                const Addr a1, const Addr a2,
                                const BmAccessTypeT access_type)
{
  Addr b, b_next;

  tl_assert(bm);

  for (b = a1; b < a2; b = b_next)
  {
    const struct bitmap2* bm2 = bm2_lookup(bm, address_msb(b));

    b_next = first_address_with_higher_msb(b);
    tl_assert(address_msb(b_next) == address_msb(b) + 1);
    if (b_next > a2)
    {
      b_next = a2;
    }

    if (bm2)
    {
      Addr b_start;
      Addr b_end;
      UWord b0;
      const struct bitmap1* const p1 = &bm2->bm1;

      if (make_address(bm2->addr, 0) < a1)
        b_start = a1;
      else
        if (make_address(bm2->addr, 0) < a2)
          b_start = make_address(bm2->addr, 0);
        else
          break;
      tl_assert(a1 <= b_start && b_start <= a2);

      if (first_address_with_higher_msb(make_address(bm2->addr, 0)) < a2)
        b_end = first_address_with_higher_msb(make_address(bm2->addr, 0));
      else
        b_end = a2;
      tl_assert(a1 <= b_end && b_end <= a2);
      tl_assert(b_start < b_end);
      tl_assert(address_lsb(b_start) <= address_lsb(b_end - 1));
      
      for (b0 = address_lsb(b_start); b0 <= address_lsb(b_end - 1); b0++)
      {
        if (access_type == eLoad)
        {
          if (bm0_is_set(p1->bm0_w, b0))
          {
            return True;
          }
        }
        else
        {
          tl_assert(access_type == eStore);
          if (bm0_is_set(p1->bm0_r, b0)
              | bm0_is_set(p1->bm0_w, b0))
          {
            return True;
          }
        }
      }
    }
  }
  return False;
}

Bool DRD_(bm_load_has_conflict_with)(struct bitmap* const bm,
                                     const Addr a1, const Addr a2)
{
  return DRD_(bm_has_conflict_with)(bm, a1, a2, eLoad);
}

Bool DRD_(bm_load_1_has_conflict_with)(struct bitmap* const bm, const Addr a1)
{
  return bm_aligned_load_has_conflict_with(bm, a1, 1);
}

Bool DRD_(bm_load_2_has_conflict_with)(struct bitmap* const bm, const Addr a1)
{
  if ((a1 & 1) == 0)
    return bm_aligned_load_has_conflict_with(bm, a1, 2);
  else
    return DRD_(bm_has_conflict_with)(bm, a1, a1 + 2, eLoad);
}

Bool DRD_(bm_load_4_has_conflict_with)(struct bitmap* const bm, const Addr a1)
{
  if ((a1 & 3) == 0)
    return bm_aligned_load_has_conflict_with(bm, a1, 4);
  else
    return DRD_(bm_has_conflict_with)(bm, a1, a1 + 4, eLoad);
}

Bool DRD_(bm_load_8_has_conflict_with)(struct bitmap* const bm, const Addr a1)
{
  if ((a1 & 7) == 0)
    return bm_aligned_load_has_conflict_with(bm, a1, 8);
  else
    return DRD_(bm_has_conflict_with)(bm, a1, a1 + 8, eLoad);
}

Bool DRD_(bm_store_1_has_conflict_with)(struct bitmap* const bm, const Addr a1)
{
  return bm_aligned_store_has_conflict_with(bm, a1, 1);
}

Bool DRD_(bm_store_2_has_conflict_with)(struct bitmap* const bm, const Addr a1)
{
  if ((a1 & 1) == 0)
    return bm_aligned_store_has_conflict_with(bm, a1, 2);
  else
    return DRD_(bm_has_conflict_with)(bm, a1, a1 + 2, eStore);
}

Bool DRD_(bm_store_4_has_conflict_with)(struct bitmap* const bm, const Addr a1)
{
  if ((a1 & 3) == 0)
    return bm_aligned_store_has_conflict_with(bm, a1, 4);
  else
    return DRD_(bm_has_conflict_with)(bm, a1, a1 + 4, eStore);
}

Bool DRD_(bm_store_8_has_conflict_with)(struct bitmap* const bm, const Addr a1)
{
  if ((a1 & 7) == 0)
    return bm_aligned_store_has_conflict_with(bm, a1, 8);
  else
    return DRD_(bm_has_conflict_with)(bm, a1, a1 + 8, eStore);
}

Bool DRD_(bm_store_has_conflict_with)(struct bitmap* const bm,
                                      const Addr a1, const Addr a2)
{
  return DRD_(bm_has_conflict_with)(bm, a1, a2, eStore);
}

/**
 * Return True if the two bitmaps *lhs and *rhs are identical, and false
 * if not.
 */
Bool DRD_(bm_equal)(struct bitmap* const lhs, struct bitmap* const rhs)
{
  struct bitmap2* bm2l;
  struct bitmap2* bm2r;

  /* It's not possible to have two independent iterators over the same OSet, */
  /* so complain if lhs == rhs.                                              */
  tl_assert(lhs != rhs);

  VG_(OSetGen_ResetIter)(lhs->oset);
  VG_(OSetGen_ResetIter)(rhs->oset);

  for ( ; (bm2l = VG_(OSetGen_Next)(lhs->oset)) != 0; )
  {
    while (bm2l
           && ! DRD_(bm_has_any_access)(lhs,
                                  make_address(bm2l->addr, 0),
                                  make_address(bm2l->addr + 1, 0)))
    {
      bm2l = VG_(OSetGen_Next)(lhs->oset);
    }
    if (bm2l == 0)
      break;
    tl_assert(bm2l);
#if 0
    VG_(message)(Vg_DebugMsg, "bm_equal: at 0x%lx",
                 make_address(bm2l->addr, 0));
#endif

    bm2r = VG_(OSetGen_Next)(rhs->oset);
    if (bm2r == 0)
    {
#if 0
      VG_(message)(Vg_DebugMsg, "bm_equal: no match found");
#endif
      return False;
    }
    tl_assert(bm2r);
    tl_assert(DRD_(bm_has_any_access)(rhs,
                                make_address(bm2r->addr, 0),
                                make_address(bm2r->addr + 1, 0)));

    if (bm2l != bm2r
        && (bm2l->addr != bm2r->addr
            || VG_(memcmp)(&bm2l->bm1, &bm2r->bm1, sizeof(bm2l->bm1)) != 0))
    {
#if 0
      VG_(message)(Vg_DebugMsg, "bm_equal: rhs 0x%lx -- returning false",
                   make_address(bm2r->addr, 0));
#endif
      return False;
    }
  }
  bm2r = VG_(OSetGen_Next)(rhs->oset);
  if (bm2r)
  {
    tl_assert(DRD_(bm_has_any_access)(rhs,
                                make_address(bm2r->addr, 0),
                                make_address(bm2r->addr + 1, 0)));
#if 0
    VG_(message)(Vg_DebugMsg,
                 "bm_equal: remaining rhs 0x%lx -- returning false",
                 make_address(bm2r->addr, 0));
#endif
    return False;
  }
  return True;
}

void DRD_(bm_swap)(struct bitmap* const bm1, struct bitmap* const bm2)
{
  OSet* const tmp = bm1->oset;
  bm1->oset = bm2->oset;
  bm2->oset = tmp;
}

/** Merge bitmaps *lhs and *rhs into *lhs. */
void DRD_(bm_merge2)(struct bitmap* const lhs,
                     struct bitmap* const rhs)
{
  struct bitmap2* bm2l;
  struct bitmap2* bm2r;

  /* It's not possible to have two independent iterators over the same OSet, */
  /* so complain if lhs == rhs.                                              */
  tl_assert(lhs != rhs);

  s_bitmap_merge_count++;

  VG_(OSetGen_ResetIter)(rhs->oset);

  for ( ; (bm2r = VG_(OSetGen_Next)(rhs->oset)) != 0; )
  {
    bm2l = VG_(OSetGen_Lookup)(lhs->oset, &bm2r->addr);
    if (bm2l)
    {
      tl_assert(bm2l != bm2r);
      bm2_merge(bm2l, bm2r);
    }
    else
    {
      bm2_insert_copy(lhs, bm2r);
    }
  }
}

/** Compute *lhs ^= *rhs. */
void bm_xor(struct bitmap* const lhs, struct bitmap* const rhs)
{
  struct bitmap2* bm2l;
  struct bitmap2* bm2r;

  /* It's not possible to have two independent iterators over the same OSet, */
  /* so complain if lhs == rhs.                                              */
  tl_assert(lhs != rhs);

  VG_(OSetGen_ResetIter)(rhs->oset);

  for ( ; (bm2r = VG_(OSetGen_Next)(rhs->oset)) != 0; )
  {
    bm2l = VG_(OSetGen_Lookup)(lhs->oset, &bm2r->addr);
    if (bm2l)
    {
      tl_assert(bm2l != bm2r);
      bm2_xor(bm2l, bm2r);
    }
    else
    {
      bm2_insert_copy(lhs, bm2r);
    }
  }
}

/**
 * Report whether there are any RW / WR / WW patterns in lhs and rhs.
 * @param lhs First bitmap.
 * @param rhs Bitmap to be compared with lhs.
 * @return !=0 if there are data races, == 0 if there are none.
 */
int DRD_(bm_has_races)(struct bitmap* const lhs, struct bitmap* const rhs)
{
  VG_(OSetGen_ResetIter)(lhs->oset);
  VG_(OSetGen_ResetIter)(rhs->oset);

  for (;;)
  {
    const struct bitmap2* bm2l;
    const struct bitmap2* bm2r;
    const struct bitmap1* bm1l;
    const struct bitmap1* bm1r;
    unsigned k;

    bm2l = VG_(OSetGen_Next)(lhs->oset);
    bm2r = VG_(OSetGen_Next)(rhs->oset);
    while (bm2l && bm2r && bm2l->addr != bm2r->addr)
    {
      if (bm2l->addr < bm2r->addr)
        bm2l = VG_(OSetGen_Next)(lhs->oset);
      else
        bm2r = VG_(OSetGen_Next)(rhs->oset);
    }
    if (bm2l == 0 || bm2r == 0)
      break;

    bm1l = &bm2l->bm1;
    bm1r = &bm2r->bm1;

    for (k = 0; k < BITMAP1_UWORD_COUNT; k++)
    {
      unsigned b;
      for (b = 0; b < BITS_PER_UWORD; b++)
      {
        UWord const access_mask
          = ((bm1l->bm0_r[k] & bm0_mask(b)) ? LHS_R : 0)
          | ((bm1l->bm0_w[k] & bm0_mask(b)) ? LHS_W : 0)
          | ((bm1r->bm0_r[k] & bm0_mask(b)) ? RHS_R : 0)
          | ((bm1r->bm0_w[k] & bm0_mask(b)) ? RHS_W : 0);
        Addr const a = make_address(bm2l->addr, k * BITS_PER_UWORD | b);
        if (HAS_RACE(access_mask) && ! DRD_(is_suppressed)(a, a + 1))
        {
          return 1;
        }
      }
    }
  }
  return 0;
}

void DRD_(bm_print)(struct bitmap* const bm)
{
  struct bitmap2* bm2;

  for (VG_(OSetGen_ResetIter)(bm->oset);
       (bm2 = VG_(OSetGen_Next)(bm->oset)) != 0;
       )
  {
    bm2_print(bm2);
  }
}

void bm2_print(const struct bitmap2* const bm2)
{
  const struct bitmap1* bm1;
  Addr a;

  tl_assert(bm2);

  bm1 = &bm2->bm1;
  for (a = make_address(bm2->addr, 0);
       a <= first_address_with_higher_msb(make_address(bm2->addr, 0)) - 1;
       a++)
  {
    const Bool r = bm0_is_set(bm1->bm0_r, address_lsb(a)) != 0;
    const Bool w = bm0_is_set(bm1->bm0_w, address_lsb(a)) != 0;
    if (r || w)
    {
      VG_(printf)("0x%08lx %c %c\n",
                  a,
                  w ? 'W' : ' ',
                  r ? 'R' : ' ');
    }
  }
}

ULong DRD_(bm_get_bitmap_creation_count)(void)
{
  return s_bitmap_creation_count;
}

ULong DRD_(bm_get_bitmap2_node_creation_count)(void)
{
  return s_bitmap_merge_count;
}

ULong DRD_(bm_get_bitmap2_creation_count)(void)
{
  return s_bitmap2_creation_count;
}


/** Clear the bitmap contents. */
void bm2_clear(struct bitmap2* const bm2)
{
  tl_assert(bm2);
  VG_(memset)(&bm2->bm1, 0, sizeof(bm2->bm1));
}

/** Compute *bm2l |= *bm2r. */
void bm2_merge(struct bitmap2* const bm2l, const struct bitmap2* const bm2r)
{
  unsigned k;

  tl_assert(bm2l);
  tl_assert(bm2r);
  tl_assert(bm2l->addr == bm2r->addr);

  s_bitmap2_merge_count++;

  for (k = 0; k < BITMAP1_UWORD_COUNT; k++)
  {
    bm2l->bm1.bm0_r[k] |= bm2r->bm1.bm0_r[k];
  }
  for (k = 0; k < BITMAP1_UWORD_COUNT; k++)
  {
    bm2l->bm1.bm0_w[k] |= bm2r->bm1.bm0_w[k];
  }
}

/** Compute *bm2l ^= *bm2r. */
void bm2_xor(struct bitmap2* const bm2l, const struct bitmap2* const bm2r)
{
  unsigned k;

  tl_assert(bm2l);
  tl_assert(bm2r);
  tl_assert(bm2l->addr == bm2r->addr);

  for (k = 0; k < BITMAP1_UWORD_COUNT; k++)
  {
    bm2l->bm1.bm0_r[k] ^= bm2r->bm1.bm0_r[k];
  }
  for (k = 0; k < BITMAP1_UWORD_COUNT; k++)
  {
    bm2l->bm1.bm0_w[k] ^= bm2r->bm1.bm0_w[k];
  }
}

ULong bm_get_bitmap2_creation_count(void)
{
  return s_bitmap2_creation_count;
}

ULong bm_get_bitmap2_merge_count(void)
{
  return s_bitmap2_merge_count;
}
