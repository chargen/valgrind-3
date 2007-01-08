
/*--------------------------------------------------------------------*/
/*--- An example Valgrind tool.                          lk_main.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Lackey, an example Valgrind tool that does
   some simple program measurement and tracing.

   Copyright (C) 2002-2006 Nicholas Nethercote
      njn@valgrind.org

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

// This tool shows how to do some basic instrumentation.
//
// There are three kinds of instrumentation it can do.  They can be turned
// on/off independently with command line options:
//
// * --basic-counts   : do basic counts, eg. number of instructions
//                      executed, jumps executed, etc.
// * --detailed-counts: do more detailed counts:  number of loads, stores
//                      and ALU operations of different sizes.
// * --trace-mem=yes:   trace all (data) memory accesses.
//
// The code for each kind of instrumentation is guarded by a clo_* variable:
// clo_basic_counts, clo_detailed_counts and clo_trace_mem.
//
// If you want to modify any of the instrumentation code, look for the code
// that is guarded by the relevant clo_* variable (eg. clo_trace_mem)
// If you're not interested in the other kinds of instrumentation you can
// remove them.  If you want to do more complex modifications, please read
// VEX/pub/libvex_ir.h to understand the intermediate representation.
//
//
// Specific Details about --trace-mem=yes
// --------------------------------------
// Lackey's --trace-mem code is a good starting point for building Valgrind
// tools that act on memory loads and stores.  It also could be used as is,
// with its output used as input to a post-mortem processing step.  However,
// because memory traces can be very large, online analysis is generally
// better.
//
// It prints memory data access traces that look like this:
//
//   instr   : 0x0023C790, 2  # instruction read at 0x0023C790 of size 2
//   instr   : 0x0023C792, 5
//     store : 0xBE80199C, 4  # data store at 0xBE80199C of size 4
//   instr   : 0x0025242B, 3
//     load  : 0xBE801950, 4  # data load at 0xBE801950 of size 4
//   instr   : 0x0023D476, 7
//     modify: 0x0025747C, 1  # data modify at 0x0025747C of size 1
//   instr   : 0x0023DC20, 2
//     load  : 0x00254962, 1
//     load  : 0xBE801FB3, 1
//   instr   : 0x00252305, 1
//     load  : 0x00254AEB, 1
//     store : 0x00257998, 1
//
// Every instruction executed has an "instr" event representing it.
// Instructions that do memory accesses are followed by one or more "load",
// "store" or "modify" events.  Some instructions do more than one load or
// store, as in the last two examples in the above trace.
//
// Here are some examples of x86 instructions that do different combinations
// of loads, stores, and modifies.
//
//    Instruction          Memory accesses                  Event sequence
//    -----------          ---------------                  --------------
//    add %eax, %ebx       No loads or stores               instr
//
//    movl (%eax), %ebx    loads (%eax)                     instr, load
//
//    movl %eax, (%ebx)    stores (%ebx)                    instr, store
//
//    incl (%ecx)          modifies (%ecx)                  instr, modify
//
//    cmpsb                loads (%esi), loads(%edi)        instr, load, load
//
//    call*l (%edx)        loads (%edx), stores -4(%esp)    instr, load, store
//    pushl (%edx)         loads (%edx), stores -4(%esp)    instr, load, store
//    movsw                loads (%esi), stores (%edi)      instr, load, store
//
// Instructions using x86 "rep" prefixes are traced as if they are repeated
// N times.
//
// Lackey with --trace-mem gives good traces, but they are not perfect, for
// the following reasons:
//
// - It does not trace into the OS kernel, so system calls and other kernel
//   operations (eg. some scheduling and signal handling code) are ignored.
//
// - It could model loads and stores done at the system call boundary using
//   the pre_mem_read/post_mem_write events.  For example, if you call
//   fstat() you know that the passed in buffer has been written.  But it
//   currently does not do this.
//
// - Valgrind replaces some code (not much) with its own, notably parts of
//   code for scheduling operations and signal handling.  This code is not
//   traced.
//
// - There is no consideration of virtual-to-physical address mapping.
//   This may not matter for many purposes.
//
// - Valgrind modifies the instruction stream in some very minor ways.  For
//   example, on x86 the bts, btc, btr instructions are incorrectly
//   considered to always touch memory (this is a consequence of these
//   instructions being very difficult to simulate).
//
// - Valgrind tools layout memory differently to normal programs, so the
//   addresses you get will not be typical.  Thus Lackey (and all Valgrind
//   tools) is suitable for getting relative memory traces -- eg. if you
//   want to analyse locality of memory accesses -- but is not good if
//   absolute addresses are important.
//
// Despite all these warnings, Dullard's results should be good enough for a
// wide range of purposes.  For example, Cachegrind shares all the above
// shortcomings and it is still useful.
//
// For further inspiration, you should look at cachegrind/cg_main.c which
// uses the same basic technique for tracing memory accesses, but also groups
// events together for processing into twos and threes so that fewer C calls
// are made and things run faster.

#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_options.h"
#include "pub_tool_machine.h"     // VG_(fnptr_to_fnentry)

/*------------------------------------------------------------*/
/*--- Command line options                                 ---*/
/*------------------------------------------------------------*/

/* Command line options controlling instrumentation kinds, as described at
 * the top of this file. */
static Bool clo_basic_counts    = True;
static Bool clo_detailed_counts = False;
static Bool clo_trace_mem       = False;

/* The name of the function of which the number of calls (under
 * --basic-counts=yes) is to be counted, with default. Override with command
 * line option --fnname. */
static Char* clo_fnname = "_dl_runtime_resolve";

static Bool lk_process_cmd_line_option(Char* arg)
{
   VG_STR_CLO(arg, "--fnname", clo_fnname)
   else VG_BOOL_CLO(arg, "--basic-counts",    clo_basic_counts)
   else VG_BOOL_CLO(arg, "--detailed-counts", clo_detailed_counts)
   else VG_BOOL_CLO(arg, "--trace-mem",       clo_trace_mem)
   else
      return False;
   
   tl_assert(clo_fnname);
   tl_assert(clo_fnname[0]);
   return True;
}

static void lk_print_usage(void)
{  
   VG_(printf)(
"    --basic-counts=no|yes     count instructions, jumps, etc. [no]\n"
"    --detailed-counts=no|yes  count loads, stores and alu ops [no]\n"
"    --trace-mem=no|yes        trace all loads and stores [no]\n"
"    --fnname=<name>           count calls to <name> (only used if\n"
"                              --basic-count=yes)  [_dl_runtime_resolve]\n"
   );
}

static void lk_print_debug_usage(void)
{  
   VG_(printf)(
"    (none)\n"
   );
}

/*------------------------------------------------------------*/
/*--- Stuff for --basic-counts                             ---*/
/*------------------------------------------------------------*/

/* Nb: use ULongs because the numbers can get very big */
static ULong n_func_calls    = 0;
static ULong n_BBs_entered   = 0;
static ULong n_BBs_completed = 0;
static ULong n_IRStmts       = 0;
static ULong n_guest_instrs  = 0;
static ULong n_Jccs          = 0;
static ULong n_Jccs_untaken  = 0;

static void add_one_func_call(void)
{
   n_func_calls++;
}

static void add_one_BB_entered(void)
{
   n_BBs_entered++;
}

static void add_one_BB_completed(void)
{
   n_BBs_completed++;
}

static void add_one_IRStmt(void)
{
   n_IRStmts++;
}

static void add_one_guest_instr(void)
{
   n_guest_instrs++;
}

static void add_one_Jcc(void)
{
   n_Jccs++;
}

static void add_one_Jcc_untaken(void)
{
   n_Jccs_untaken++;
}

/*------------------------------------------------------------*/
/*--- Stuff for --detailed-counts                          ---*/
/*------------------------------------------------------------*/

/* --- Operations --- */

typedef enum { OpLoad=0, OpStore=1, OpAlu=2 } Op;

#define N_OPS 3


/* --- Types --- */

#define N_TYPES 9

static Int type2index ( IRType ty )
{
   switch (ty) {
      case Ity_I1:      return 0;
      case Ity_I8:      return 1;
      case Ity_I16:     return 2;
      case Ity_I32:     return 3;
      case Ity_I64:     return 4;
      case Ity_I128:    return 5;
      case Ity_F32:     return 6;
      case Ity_F64:     return 7;
      case Ity_V128:    return 8;
      default: tl_assert(0); break;
   }
}

static HChar* nameOfTypeIndex ( IRType ty )
{
   switch (ty) {
      case 0: return "I1";   break;
      case 1: return "I8";   break;
      case 2: return "I16";  break;
      case 3: return "I32";  break;
      case 4: return "I64";  break;
      case 5: return "I128"; break;
      case 6: return "F32";  break;
      case 7: return "F64";  break;
      case 8: return "V128"; break;
      default: tl_assert(0); break;
   }
}


/* --- Counts --- */

static ULong detailCounts[N_OPS][N_TYPES];

/* The helper that is called from the instrumented code. */
static VG_REGPARM(1)
void increment_detail(ULong* detail)
{
   (*detail)++;
}

/* A helper that adds the instrumentation for a detail. */
static void instrument_detail(IRSB* bb, Op op, IRType type)
{
   IRDirty* di;
   IRExpr** argv;
   const UInt typeIx = type2index(type);

   tl_assert(op < N_OPS);
   tl_assert(typeIx < N_TYPES);

   argv = mkIRExprVec_1( mkIRExpr_HWord( (HWord)&detailCounts[op][typeIx] ) );
   di = unsafeIRDirty_0_N( 1, "increment_detail",
                              VG_(fnptr_to_fnentry)( &increment_detail ), 
                              argv);
   addStmtToIRSB( bb, IRStmt_Dirty(di) );
}

/* Summarize and print the details. */
static void print_details ( void )
{
   Int typeIx;
   VG_(message)(Vg_UserMsg,
                "   Type        Loads       Stores       AluOps");
   VG_(message)(Vg_UserMsg,
                "   -------------------------------------------");
   for (typeIx = 0; typeIx < N_TYPES; typeIx++) {
      VG_(message)(Vg_UserMsg,
                   "   %4s %,12llu %,12llu %,12llu", 
                   nameOfTypeIndex( typeIx ),
                   detailCounts[OpLoad ][typeIx],
                   detailCounts[OpStore][typeIx],
                   detailCounts[OpAlu  ][typeIx]
      );
   }
}


/*------------------------------------------------------------*/
/*--- Stuff for --trace-mem                                ---*/
/*------------------------------------------------------------*/

#define MAX_DSIZE    512

typedef
   IRExpr 
   IRAtom;

typedef 
   enum { Event_Ir, Event_Dr, Event_Dw, Event_Dm }
   EventKind;

typedef
   struct {
      EventKind  ekind;
      IRAtom*    addr;
      Int        size;
   }
   Event;

/* Up to this many unnotified events are allowed.  Must be at least two,
   so that reads and writes to the same address can be merged into a modify.
   Beyond that, larger numbers just potentially induce more spilling due to
   extending live ranges of address temporaries. */
#define N_EVENTS 4

/* Maintain an ordered list of memory events which are outstanding, in
   the sense that no IR has yet been generated to do the relevant
   helper calls.  The BB is scanned top to bottom and memory events
   are added to the end of the list, merging with the most recent
   notified event where possible (Dw immediately following Dr and
   having the same size and EA can be merged).

   This merging is done so that for architectures which have
   load-op-store instructions (x86, amd64), the instr is treated as if
   it makes just one memory reference (a modify), rather than two (a
   read followed by a write at the same address).

   At various points the list will need to be flushed, that is, IR
   generated from it.  That must happen before any possible exit from
   the block (the end, or an IRStmt_Exit).  Flushing also takes place
   when there is no space to add a new event.

   If we require the simulation statistics to be up to date with
   respect to possible memory exceptions, then the list would have to
   be flushed before each memory reference.  That's a pain so we don't
   bother.

   Flushing the list consists of walking it start to end and emitting
   instrumentation IR for each event, in the order in which they
   appear. */

static Event events[N_EVENTS];
static Int   events_used = 0;


static VG_REGPARM(2) void trace_instr(Addr addr, SizeT size)
{
   VG_(printf)("instr   : %08p, %d\n", addr, size);
}

static VG_REGPARM(2) void trace_load(Addr addr, SizeT size)
{
   VG_(printf)("  load  : %08p, %d\n", addr, size);
}

static VG_REGPARM(2) void trace_store(Addr addr, SizeT size)
{
   VG_(printf)("  store : %08p, %d\n", addr, size);
}

static VG_REGPARM(2) void trace_modify(Addr addr, SizeT size)
{
   VG_(printf)("  modify: %08p, %d\n", addr, size);
}


static void flushEvents(IRSB* bb)
{
   Int        i;
   Char*      helperName;
   void*      helperAddr;
   IRExpr**   argv;
   IRDirty*   di;
   Event*     ev;

   for (i = 0; i < events_used; i++) {

      ev = &events[i];
      
      // Decide on helper fn to call and args to pass it.
      switch (ev->ekind) {
         case Event_Ir: helperName = "trace_instr";
                        helperAddr =  trace_instr;  break;

         case Event_Dr: helperName = "trace_load";
                        helperAddr =  trace_load;   break;

         case Event_Dw: helperName = "trace_store";
                        helperAddr =  trace_store;  break;

         case Event_Dm: helperName = "trace_modify";
                        helperAddr =  trace_modify; break;
         default:
            tl_assert(0);
      }

      // Add the helper.
      argv = mkIRExprVec_2( ev->addr, mkIRExpr_HWord( ev->size ) );
      di   = unsafeIRDirty_0_N( /*regparms*/2, 
                                helperName, VG_(fnptr_to_fnentry)( helperAddr ),
                                argv );
      addStmtToIRSB( bb, IRStmt_Dirty(di) );
   }

   events_used = 0;
}

// WARNING:  If you aren't interested in instruction reads, you can omit the
// code that adds calls to trace_instr() in flushEvents().  However, you
// must still call this function, addEvent_Ir() -- it is necessary to add
// the Ir events to the events list so that merging of paired load/store
// events into modify events works correctly.
static void addEvent_Ir ( IRSB* bb, IRAtom* iaddr, UInt isize )
{
   Event* evt;
   tl_assert( (VG_MIN_INSTR_SZB <= isize && isize <= VG_MAX_INSTR_SZB)
            || VG_CLREQ_SZB == isize );
   if (events_used == N_EVENTS)
      flushEvents(bb);
   tl_assert(events_used >= 0 && events_used < N_EVENTS);
   evt = &events[events_used];
   evt->ekind = Event_Ir;
   evt->addr  = iaddr;
   evt->size  = isize;
   events_used++;
}

static
void addEvent_Dr ( IRSB* bb, IRAtom* daddr, Int dsize )
{
   Event* evt;
   tl_assert(isIRAtom(daddr));
   tl_assert(dsize >= 1 && dsize <= MAX_DSIZE);
   if (events_used == N_EVENTS)
      flushEvents(bb);
   tl_assert(events_used >= 0 && events_used < N_EVENTS);
   evt = &events[events_used];
   evt->ekind = Event_Dr;
   evt->addr  = daddr;
   evt->size  = dsize;
   events_used++;
}

static
void addEvent_Dw ( IRSB* bb, IRAtom* daddr, Int dsize )
{
   Event* lastEvt;
   Event* evt;
   tl_assert(isIRAtom(daddr));
   tl_assert(dsize >= 1 && dsize <= MAX_DSIZE);

   // Is it possible to merge this write with the preceding read?
   lastEvt = &events[events_used-1];
   if (events_used > 0
    && lastEvt->ekind == Event_Dr
    && lastEvt->size  == dsize
    && eqIRAtom(lastEvt->addr, daddr))
   {
      lastEvt->ekind = Event_Dm;
      return;
   }

   // No.  Add as normal.
   if (events_used == N_EVENTS)
      flushEvents(bb);
   tl_assert(events_used >= 0 && events_used < N_EVENTS);
   evt = &events[events_used];
   evt->ekind = Event_Dw;
   evt->size  = dsize;
   evt->addr  = daddr;
   events_used++;
}


/*------------------------------------------------------------*/
/*--- Basic tool functions                                 ---*/
/*------------------------------------------------------------*/

static void lk_post_clo_init(void)
{
   Int op, tyIx;

   if (clo_detailed_counts) {
      for (op = 0; op < N_OPS; op++)
         for (tyIx = 0; tyIx < N_TYPES; tyIx++)
            detailCounts[op][tyIx] = 0;
   }
}

static
IRSB* lk_instrument ( VgCallbackClosure* closure,
                      IRSB* bbIn, 
                      VexGuestLayout* layout, 
                      VexGuestExtents* vge,
                      IRType gWordTy, IRType hWordTy )
{
   IRDirty*   di;
   Int        i;
   IRSB*      bbOut;
   Char       fnname[100];
   IRType     type;
   IRTypeEnv* tyenv = bbIn->tyenv;

   if (gWordTy != hWordTy) {
      /* We don't currently support this case. */
      VG_(tool_panic)("host/guest word size mismatch");
   }

   /* Set up BB */
   bbOut = deepCopyIRSBExceptStmts(bbIn);

   // Copy verbatim any IR preamble preceding the first IMark
   i = 0;
   while (i < bbIn->stmts_used && bbIn->stmts[i]->tag != Ist_IMark) {
      addStmtToIRSB( bbOut, bbIn->stmts[i] );
      i++;
   }

   if (clo_basic_counts) {
      /* Count this basic block. */
      di = unsafeIRDirty_0_N( 0, "add_one_BB_entered", 
                                 VG_(fnptr_to_fnentry)( &add_one_BB_entered ),
                                 mkIRExprVec_0() );
      addStmtToIRSB( bbOut, IRStmt_Dirty(di) );
   }

   if (clo_trace_mem) {
      events_used = 0;
   }

   for (/*use current i*/; i < bbIn->stmts_used; i++) {
      IRStmt* st = bbIn->stmts[i];
      if (!st || st->tag == Ist_NoOp) continue;

      if (clo_basic_counts) {
         /* Count one VEX statement. */
         di = unsafeIRDirty_0_N( 0, "add_one_IRStmt", 
                                    VG_(fnptr_to_fnentry)( &add_one_IRStmt ), 
                                    mkIRExprVec_0() );
         addStmtToIRSB( bbOut, IRStmt_Dirty(di) );
      }
      
      switch (st->tag) {
         case Ist_NoOp:
         case Ist_AbiHint:
         case Ist_Put:
         case Ist_PutI:
         case Ist_MFence:
            addStmtToIRSB( bbOut, st );
            break;

         case Ist_IMark:
            if (clo_basic_counts) {
               /* Count guest instruction. */
               di = unsafeIRDirty_0_N( 0, "add_one_guest_instr",
                                          VG_(fnptr_to_fnentry)( &add_one_guest_instr ), 
                                          mkIRExprVec_0() );
               addStmtToIRSB( bbOut, IRStmt_Dirty(di) );

               /* An unconditional branch to a known destination in the
                * guest's instructions can be represented, in the IRSB to
                * instrument, by the VEX statements that are the
                * translation of that known destination. This feature is
                * called 'BB chasing' and can be influenced by command
                * line option --vex-guest-chase-thresh.
                *
                * To get an accurate count of the calls to a specific
                * function, taking BB chasing into account, we need to
                * check for each guest instruction (Ist_IMark) if it is
                * the entry point of a function.
                */
               tl_assert(clo_fnname);
               tl_assert(clo_fnname[0]);
               if (VG_(get_fnname_if_entry)(st->Ist.IMark.addr, 
                                            fnname, sizeof(fnname))
                   && 0 == VG_(strcmp)(fnname, clo_fnname)) {
                  di = unsafeIRDirty_0_N( 
                          0, "add_one_func_call", 
                             VG_(fnptr_to_fnentry)( &add_one_func_call ), 
                             mkIRExprVec_0() );
                  addStmtToIRSB( bbOut, IRStmt_Dirty(di) );
               }
            }
            if (clo_trace_mem) {
               // WARNING: do not remove this function call, even if you
               // aren't interested in instruction reads.  See the comment
               // above the function itself for more detail.
               addEvent_Ir( bbOut, mkIRExpr_HWord( (HWord)st->Ist.IMark.addr ),
                            st->Ist.IMark.len );
            }
            addStmtToIRSB( bbOut, st );
            break;

         case Ist_WrTmp:
            // Add a call to trace_load() if --trace-mem=yes.
            if (clo_trace_mem) {
               IRExpr* data = st->Ist.WrTmp.data;
               if (data->tag == Iex_Load) {
                  addEvent_Dr( bbOut, data->Iex.Load.addr,
                               sizeofIRType(data->Iex.Load.ty) );
               }
            }
            if (clo_detailed_counts) {
               IRExpr* expr = st->Ist.WrTmp.data;
               type = typeOfIRExpr(bbOut->tyenv, expr);
               tl_assert(type != Ity_INVALID);
               switch (expr->tag) {
                  case Iex_Load:
                     instrument_detail( bbOut, OpLoad, type );
                     break;
                  case Iex_Unop:
                  case Iex_Binop:
                  case Iex_Triop:
                  case Iex_Qop:
                  case Iex_Mux0X:
                     instrument_detail( bbOut, OpAlu, type );
                     break;
                  default:
                     break;
               }
            }
            addStmtToIRSB( bbOut, st );
            break;

         case Ist_Store:
            if (clo_trace_mem) {
               IRExpr* data  = st->Ist.Store.data;
               addEvent_Dw( bbOut, st->Ist.Store.addr,
                            sizeofIRType(typeOfIRExpr(tyenv, data)) );
            }
            if (clo_detailed_counts) {
               type = typeOfIRExpr(bbOut->tyenv, st->Ist.Store.data);
               tl_assert(type != Ity_INVALID);
               instrument_detail( bbOut, OpStore, type );
            }
            addStmtToIRSB( bbOut, st );
            break;

         case Ist_Dirty: {
            Int      dsize;
            IRDirty* d = st->Ist.Dirty.details;
            if (d->mFx != Ifx_None) {
               // This dirty helper accesses memory.  Collect the details.
               tl_assert(d->mAddr != NULL);
               tl_assert(d->mSize != 0);
               dsize = d->mSize;
               if (d->mFx == Ifx_Read || d->mFx == Ifx_Modify)
                  addEvent_Dr( bbOut, d->mAddr, dsize );
               if (d->mFx == Ifx_Write || d->mFx == Ifx_Modify)
                  addEvent_Dw( bbOut, d->mAddr, dsize );
            } else {
               tl_assert(d->mAddr == NULL);
               tl_assert(d->mSize == 0);
            }
            addStmtToIRSB( bbOut, st );
            break;
         }

         case Ist_Exit:
            if (clo_basic_counts) {
               /* Count Jcc */
               di = unsafeIRDirty_0_N( 0, "add_one_Jcc", 
                                          VG_(fnptr_to_fnentry)( &add_one_Jcc ), 
                                          mkIRExprVec_0() );
               addStmtToIRSB( bbOut, IRStmt_Dirty(di) );
            }
            if (clo_trace_mem) {
               flushEvents(bbOut);
            }

            addStmtToIRSB( bbOut, st );      // Original statement

            if (clo_basic_counts) {
               /* Count non-taken Jcc */
               di = unsafeIRDirty_0_N( 0, "add_one_Jcc_untaken", 
                                          VG_(fnptr_to_fnentry)(
                                             &add_one_Jcc_untaken ),
                                          mkIRExprVec_0() );
               addStmtToIRSB( bbOut, IRStmt_Dirty(di) );
            }
            break;

         default:
            tl_assert(0);
      }
   }

   if (clo_basic_counts) {
      /* Count this basic block. */
      di = unsafeIRDirty_0_N( 0, "add_one_BB_completed", 
                                 VG_(fnptr_to_fnentry)( &add_one_BB_completed ),
                                 mkIRExprVec_0() );
      addStmtToIRSB( bbOut, IRStmt_Dirty(di) );
   }

   if (clo_trace_mem) {
      /* At the end of the bbIn.  Flush outstandings. */
      flushEvents(bbOut);
   }

   return bbOut;
}

static void lk_fini(Int exitcode)
{
   char percentify_buf[4]; /* Two digits, '%' and 0. */
   const int percentify_size = sizeof(percentify_buf);
   const int percentify_decs = 0;
   
   tl_assert(clo_fnname);
   tl_assert(clo_fnname[0]);

   if (clo_basic_counts) {
      VG_(message)(Vg_UserMsg,
         "Counted %,llu calls to %s()", n_func_calls, clo_fnname);

      VG_(message)(Vg_UserMsg, "");
      VG_(message)(Vg_UserMsg, "Jccs:");
      VG_(message)(Vg_UserMsg, "  total:         %,llu", n_Jccs);
      VG_(percentify)((n_Jccs - n_Jccs_untaken), (n_Jccs ? n_Jccs : 1),
         percentify_decs, percentify_size, percentify_buf);
      VG_(message)(Vg_UserMsg, "  taken:         %,llu (%s)", 
         (n_Jccs - n_Jccs_untaken), percentify_buf);
      
      VG_(message)(Vg_UserMsg, "");
      VG_(message)(Vg_UserMsg, "Executed:");
      VG_(message)(Vg_UserMsg, "  BBs entered:   %,llu", n_BBs_entered);
      VG_(message)(Vg_UserMsg, "  BBs completed: %,llu", n_BBs_completed);
      VG_(message)(Vg_UserMsg, "  guest instrs:  %,llu", n_guest_instrs);
      VG_(message)(Vg_UserMsg, "  IRStmts:       %,llu", n_IRStmts);
      
      VG_(message)(Vg_UserMsg, "");
      VG_(message)(Vg_UserMsg, "Ratios:");
      tl_assert(n_BBs_entered); // Paranoia time.
      VG_(message)(Vg_UserMsg, "  guest instrs : BB entered  = %3u : 10",
         10 * n_guest_instrs / n_BBs_entered);
      VG_(message)(Vg_UserMsg, "       IRStmts : BB entered  = %3u : 10",
         10 * n_IRStmts / n_BBs_entered);
      tl_assert(n_guest_instrs); // Paranoia time.
      VG_(message)(Vg_UserMsg, "       IRStmts : guest instr = %3u : 10",
         10 * n_IRStmts / n_guest_instrs);
   }

   if (clo_detailed_counts) {
      VG_(message)(Vg_UserMsg, "");
      VG_(message)(Vg_UserMsg, "IR-level counts by type:");
      print_details();
   }

   if (clo_basic_counts) {
      VG_(message)(Vg_UserMsg, "");
      VG_(message)(Vg_UserMsg, "Exit code:       %d", exitcode);
   }
}

static void lk_pre_clo_init(void)
{
   VG_(details_name)            ("Lackey");
   VG_(details_version)         (NULL);
   VG_(details_description)     ("an example Valgrind tool");
   VG_(details_copyright_author)(
      "Copyright (C) 2002-2006, and GNU GPL'd, by Nicholas Nethercote.");
   VG_(details_bug_reports_to)  (VG_BUGS_TO);
   VG_(details_avg_translation_sizeB) ( 200 );

   VG_(basic_tool_funcs)          (lk_post_clo_init,
                                   lk_instrument,
                                   lk_fini);
   VG_(needs_command_line_options)(lk_process_cmd_line_option,
                                   lk_print_usage,
                                   lk_print_debug_usage);
}

VG_DETERMINE_INTERFACE_VERSION(lk_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                lk_main.c ---*/
/*--------------------------------------------------------------------*/
