
#include <stdio.h>
#include <errno.h>
#include <string.h>

/* Stuff from Wine. */

typedef  unsigned short  WORD;  /* I guess */
typedef  unsigned char   BYTE;

typedef struct _LDT_ENTRY {
    WORD        LimitLow;
    WORD        BaseLow;
    union {
        struct {
            BYTE        BaseMid;
            BYTE        Flags1;
               /*Declare as bytes to avoid alignment problems */
            BYTE        Flags2;
            BYTE        BaseHi;
        } Bytes;
        struct {
            unsigned    BaseMid         : 8;
            unsigned    Type            : 5;
            unsigned    Dpl             : 2;
            unsigned    Pres            : 1;
            unsigned    LimitHi         : 4;
            unsigned    Sys             : 1;
            unsigned    Reserved_0      : 1;
            unsigned    Default_Big     : 1;
            unsigned    Granularity     : 1;
            unsigned    BaseHi          : 8;
        } Bits;
    } HighWord;
} LDT_ENTRY;

inline static void *wine_ldt_get_base( const LDT_ENTRY *ent )
{
    return (void *)(ent->BaseLow |
                    (unsigned long)ent->HighWord.Bits.BaseMid << 16 |
                    (unsigned long)ent->HighWord.Bits.BaseHi << 24);
}
inline static unsigned int wine_ldt_get_limit( const LDT_ENTRY *ent )
{
    unsigned int limit = ent->LimitLow | (ent->HighWord.Bits.LimitHi << 16);
    if (ent->HighWord.Bits.Granularity) limit = (limit << 12) | 0xfff;
    return limit;
}


/* our copy of the ldt */
LDT_ENTRY ldt_copy[8192];

/* System call to set LDT entry.  */
//extern int __modify_ldt (int, struct modify_ldt_ldt_s *, size_t);
extern int __modify_ldt (int, void *, size_t);

void print_ldt ( void )
{
   int res;
   res = __modify_ldt( 0, ldt_copy, 8192*sizeof(LDT_ENTRY) );
   printf("got %d bytes\n", res );   
   perror("error is");
}

/* Structure passed on `modify_ldt' call.  */
#define MODIFY_LDT_CONTENTS_DATA        0
#define MODIFY_LDT_CONTENTS_STACK       1
#define MODIFY_LDT_CONTENTS_CODE        2

struct modify_ldt_ldt_s
{
  unsigned int entry_number;
  unsigned long int base_addr;
  unsigned int limit;
  unsigned int seg_32bit:1;
  unsigned int contents:2;
  unsigned int read_exec_only:1;
  unsigned int limit_in_pages:1;
  unsigned int seg_not_present:1;
  unsigned int useable:1;
  unsigned int empty:25;
};

/* System call to set LDT entry.  */
//extern int __modify_ldt (int, struct modify_ldt_ldt_s *, size_t);

void set_ldt1 ( void* base )
{
  int stat;
  struct modify_ldt_ldt_s ldt_entry;

  /* stop valgrind yelping about initialised holes in this struct. */
  memset(&ldt_entry, 0, sizeof(ldt_entry));

  ldt_entry.entry_number = 1;
  ldt_entry.base_addr = (unsigned)base; //0x12345678;
  ldt_entry.limit = 10;
  ldt_entry.seg_32bit = 1;
  ldt_entry.contents = MODIFY_LDT_CONTENTS_DATA;
  ldt_entry.read_exec_only = 0;
  ldt_entry.limit_in_pages = 0;
  ldt_entry.seg_not_present = 0;
  stat = __modify_ldt (1, &ldt_entry, sizeof (ldt_entry));
  printf("stat = %d\n", stat);
}


void ldt_seg_write ( int ldt_entno, unsigned offset, unsigned val )
{
  asm volatile("movl %2, %%eax\n\t"
               "movl %1, %%edx\n\t"
	       "movl %0, %%gs\n\t"
               "movl %%eax, %%gs:(%%edx)\t"
	       : 
               : "r" (7 /* LDT(TI), least privilege */ + (ldt_entno << 3)), 
                 "r" (offset), "r" (val)
               : "eax", "edx", "cc" );
}

int main ( void )
{
  int i;
  int arr[9];

  for (i = 0; i < 9; i++) arr[i] = 11*i;

  set_ldt1( &arr[4] );
  print_ldt();

  ldt_seg_write(1 /* ldt entry # */, 4 /* offset */, 4444);

  for (i = 0; i < 9; i++) printf("%d ", arr[i]);
  printf("\n");

  return 0;
}

