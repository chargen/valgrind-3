
/*--------------------------------------------------------------------*/
/*--- Storage, and equality on, execution contexts (backtraces).   ---*/
/*---                                              vg_execontext.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Valgrind, an extensible x86 protected-mode
   emulator for monitoring program execution on x86-Unixes.

   Copyright (C) 2000-2004 Julian Seward 
      jseward@acm.org

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

#include "vg_include.h"


/*------------------------------------------------------------*/
/*--- Low-level ExeContext storage.                        ---*/
/*------------------------------------------------------------*/

/* The idea is only to ever store any one context once, so as to save
   space and make exact comparisons faster. */

static ExeContext* vg_ec_list[VG_N_EC_LISTS];

/* Stats only: the number of times the system was searched to locate a
   context. */
static UInt vg_ec_searchreqs;

/* Stats only: the number of full context comparisons done. */
static UInt vg_ec_searchcmps;

/* Stats only: total number of stored contexts. */
static UInt vg_ec_totstored;

/* Number of 2, 4 and (fast) full cmps done. */
static UInt vg_ec_cmp2s;
static UInt vg_ec_cmp4s;
static UInt vg_ec_cmpAlls;


/*------------------------------------------------------------*/
/*--- Exported functions.                                  ---*/
/*------------------------------------------------------------*/


/* Initialise this subsystem. */
static void init_ExeContext_storage ( void )
{
   Int i;
   static Bool init_done = False;
   if (init_done)
      return;
   vg_ec_searchreqs = 0;
   vg_ec_searchcmps = 0;
   vg_ec_totstored = 0;
   vg_ec_cmp2s = 0;
   vg_ec_cmp4s = 0;
   vg_ec_cmpAlls = 0;
   for (i = 0; i < VG_N_EC_LISTS; i++)
      vg_ec_list[i] = NULL;
   init_done = True;
}


/* Show stats. */
void VG_(show_ExeContext_stats) ( void )
{
   init_ExeContext_storage();
   VG_(message)(Vg_DebugMsg, 
      "exectx: %d lists, %d contexts (avg %d per list)",
      VG_N_EC_LISTS, vg_ec_totstored, 
      vg_ec_totstored / VG_N_EC_LISTS 
   );
   VG_(message)(Vg_DebugMsg, 
      "exectx: %d searches, %d full compares (%d per 1000)",
      vg_ec_searchreqs, vg_ec_searchcmps, 
      vg_ec_searchreqs == 0 
         ? 0 
         : (UInt)( (((ULong)vg_ec_searchcmps) * 1000) 
           / ((ULong)vg_ec_searchreqs )) 
   );
   VG_(message)(Vg_DebugMsg, 
      "exectx: %d cmp2, %d cmp4, %d cmpAll",
      vg_ec_cmp2s, vg_ec_cmp4s, vg_ec_cmpAlls 
   );
}


/* Print an ExeContext. */
void VG_(pp_ExeContext) ( ExeContext* e )
{
   init_ExeContext_storage();
   VG_(mini_stack_dump) ( e->eips, VG_(clo_backtrace_size) );
}


/* Compare two ExeContexts, comparing all callers. */
Bool VG_(eq_ExeContext) ( VgRes res, ExeContext* e1, ExeContext* e2 )
{
   if (e1 == NULL || e2 == NULL) 
      return False;
   switch (res) {
   case Vg_LowRes:
      /* Just compare the top two callers. */
      vg_ec_cmp2s++;
      if (e1->eips[0] != e2->eips[0]
          || e1->eips[1] != e2->eips[1]) return False;
      return True;

   case Vg_MedRes:
      /* Just compare the top four callers. */
      vg_ec_cmp4s++;
      if (e1->eips[0] != e2->eips[0]) return False;

      if (VG_(clo_backtrace_size) < 2) return True;
      if (e1->eips[1] != e2->eips[1]) return False;

      if (VG_(clo_backtrace_size) < 3) return True;
      if (e1->eips[2] != e2->eips[2]) return False;

      if (VG_(clo_backtrace_size) < 4) return True;
      if (e1->eips[3] != e2->eips[3]) return False;
      return True;

   case Vg_HighRes:
      vg_ec_cmpAlls++;
      /* Compare them all -- just do pointer comparison. */
      if (e1 != e2) return False;
      return True;

   default:
      VG_(core_panic)("VG_(eq_ExeContext): unrecognised VgRes");
   }
}


/* Take a snapshot of the client's stack, putting the up to 'n_eips' %eips
   into 'eips'.  In order to be thread-safe, we pass in the thread's %EIP
   and %EBP.  Returns number of %eips put in 'eips'.  */
static UInt stack_snapshot2 ( Addr* eips, UInt n_eips, Addr eip, Addr ebp,
                              Addr ebp_min, Addr ebp_max_orig )
{
   Int         i;
   Addr        ebp_max;
   UInt        n_found = 0;

   VGP_PUSHCC(VgpExeContext);

   /* First snaffle %EIPs from the client's stack into eips[0 .. n_eips-1], 
      putting zeroes in when the trail goes cold, which we guess to be when
      %ebp is not a reasonable stack location.  We also assert that %ebp
      increases down the chain. */

   // Gives shorter stack trace for tests/badjump.c
   // JRS 2002-aug-16: I don't think this is a big deal; looks ok for
   // most "normal" backtraces.
   // NJN 2002-sep-05: traces for pthreaded programs are particularly bad.

   // JRS 2002-sep-17: hack, to round up ebp_max to the end of the
   // current page, at least.  Dunno if it helps.
   // NJN 2002-sep-17: seems to -- stack traces look like 1.0.X again
   ebp_max = (ebp_max_orig + VKI_BYTES_PER_PAGE - 1) 
                & ~(VKI_BYTES_PER_PAGE - 1);
   ebp_max -= sizeof(Addr);

   /* Assertion broken before main() is reached in pthreaded programs;  the
    * offending stack traces only have one item.  --njn, 2002-aug-16 */
   /* vg_assert(ebp_min <= ebp_max);*/

   if (ebp_min + 4000000 <= ebp_max) {
      /* If the stack is ridiculously big, don't poke around ... but
         don't bomb out either.  Needed to make John Regehr's
         user-space threads package work. JRS 20021001 */
      eips[0] = eip;
      i = 1;
   } else {
      /* Get whatever we safely can ... */
      eips[0] = eip;
      for (i = 1; i < n_eips; i++) {
         if (!(ebp_min <= ebp && ebp <= ebp_max)) {
            //VG_(printf)("... out of range %p\n", ebp);
            break; /* ebp gone baaaad */
         }
         // NJN 2002-sep-17: monotonicity doesn't work -- gives wrong traces...
         //     if (ebp >= ((UInt*)ebp)[0]) {
         //   VG_(printf)("nonmonotonic\n");
         //    break; /* ebp gone nonmonotonic */
         // }
         eips[i] = ((UInt*)ebp)[1];  /* ret addr */
         ebp     = ((UInt*)ebp)[0];  /* old ebp */
         //VG_(printf)("     %p\n", eips[i]);
      }
   }
   n_found = i;

   /* Put zeroes in the rest. */
   for (;  i < n_eips; i++) {
      eips[i] = 0;
   }
   VGP_POPCC(VgpExeContext);

   return n_found;
}

/* This guy is the head honcho here.  Take a snapshot of the client's
   stack.  Search our collection of ExeContexts to see if we already
   have it, and if not, allocate a new one.  Either way, return a
   pointer to the context.  If there is a matching context we
   guarantee to not allocate a new one.  Thus we never store
   duplicates, and so exact equality can be quickly done as equality
   on the returned ExeContext* values themselves.  Inspired by Hugs's
   Text type.  
*/
ExeContext* VG_(get_ExeContext2) ( Addr eip, Addr ebp,
                                   Addr ebp_min, Addr ebp_max_orig )
{
   Int         i;
   Addr        eips[VG_DEEPEST_BACKTRACE];
   Bool        same;
   UInt        hash;
   ExeContext* new_ec;
   ExeContext* list;

   VGP_PUSHCC(VgpExeContext);

   init_ExeContext_storage();
   vg_assert(VG_(clo_backtrace_size) >= 1 
             && VG_(clo_backtrace_size) <= VG_DEEPEST_BACKTRACE);

   stack_snapshot2( eips, VG_(clo_backtrace_size),
                    eip, ebp, ebp_min, ebp_max_orig );

   /* Now figure out if we've seen this one before.  First hash it so
      as to determine the list number. */

   hash = 0;
   for (i = 0; i < VG_(clo_backtrace_size); i++) {
      hash ^= (UInt)eips[i];
      hash = (hash << 29) | (hash >> 3);
   }
   hash = hash % VG_N_EC_LISTS;

   /* And (the expensive bit) look a matching entry in the list. */

   vg_ec_searchreqs++;

   list = vg_ec_list[hash];

   while (True) {
      if (list == NULL) break;
      vg_ec_searchcmps++;
      same = True;
      for (i = 0; i < VG_(clo_backtrace_size); i++) {
         if (list->eips[i] != eips[i]) {
            same = False;
            break; 
         }
      }
      if (same) break;
      list = list->next;
   }

   if (list != NULL) {
      /* Yay!  We found it.  */
      VGP_POPCC(VgpExeContext);
      return list;
   }

   /* Bummer.  We have to allocate a new context record. */
   vg_ec_totstored++;

   new_ec = VG_(arena_malloc)( VG_AR_EXECTXT, 
                               sizeof(struct _ExeContext *) 
                               + VG_(clo_backtrace_size) * sizeof(Addr) );

   for (i = 0; i < VG_(clo_backtrace_size); i++)
      new_ec->eips[i] = eips[i];

   new_ec->next = vg_ec_list[hash];
   vg_ec_list[hash] = new_ec;

   VGP_POPCC(VgpExeContext);
   return new_ec;
}

void get_needed_regs(ThreadId tid, Addr* eip, Addr* ebp, Addr* esp,
                     Addr* stack_highest_word)
{
   if (VG_(is_running_thread)(tid)) {
      /* thread currently in baseblock */
      *eip                = VG_(baseBlock)[VGOFF_(m_eip)];
      *ebp                = VG_(baseBlock)[VGOFF_(m_ebp)];
      *esp                = VG_(baseBlock)[VGOFF_(m_esp)];
      *stack_highest_word = VG_(threads)[tid].stack_highest_word;
   } else {
      /* thread in thread table */
      ThreadState* tst = & VG_(threads)[ tid ];
      *eip                = tst->m_eip;
      *ebp                = tst->m_ebp;
      *esp                = tst->m_esp; 
      *stack_highest_word = tst->stack_highest_word;
   }

   /* Nasty little hack to deal with sysinfo syscalls - if libc is
      using the sysinfo page for syscalls (the TLS version does), then
      eip will always appear to be in that page when doing a syscall,
      not the actual libc function doing the syscall.  This check sees
      if EIP is within the syscall code, and pops the return address
      off the stack so that eip is placed within the library function
      calling the syscall.  This makes stack backtraces much more
      useful.  */
   if (*eip >= VG_(client_trampoline_code)+VG_(tramp_syscall_offset) &&
       *eip < VG_(client_trampoline_code)+VG_(trampoline_code_length) &&
       VG_(is_addressable)(*esp, sizeof(Addr))) {
      *eip = *(Addr *)*esp;
      *esp += sizeof(Addr);
   }
}

ExeContext* VG_(get_ExeContext) ( ThreadId tid )
{
   Addr eip, ebp, esp, stack_highest_word;

   get_needed_regs(tid, &eip, &ebp, &esp, &stack_highest_word);
   return VG_(get_ExeContext2)(eip, ebp, esp, stack_highest_word);
}

/* Take a snapshot of the client's stack, putting the up to 'n_eips' %eips
   into 'eips'.  In order to be thread-safe, we pass in the thread's %EIP
   and %EBP.  Returns number of %eips put in 'eips'.  */
UInt VG_(stack_snapshot) ( ThreadId tid, Addr* eips, UInt n_eips )
{
   Addr eip, ebp, esp, stack_highest_word;

   get_needed_regs(tid, &eip, &ebp, &esp, &stack_highest_word);
   return stack_snapshot2(eips, n_eips, 
                          eip, ebp, esp, stack_highest_word);
}


Addr VG_(get_EIP_from_ExeContext) ( ExeContext* e, UInt n )
{
   if (n > VG_(clo_backtrace_size)) return 0;
   return e->eips[n];
}

Addr VG_(get_EIP) ( ThreadId tid )
{
   Addr ret;

   if (VG_(is_running_thread)(tid))
      ret = VG_(baseBlock)[VGOFF_(m_eip)];
   else
      ret = VG_(threads)[ tid ].m_eip;

   return ret;
}

/*--------------------------------------------------------------------*/
/*--- end                                          vg_execontext.c ---*/
/*--------------------------------------------------------------------*/
