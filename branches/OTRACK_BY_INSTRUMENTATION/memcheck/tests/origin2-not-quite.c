
/* This test case was originally written by Nicholas Nethercote. */

// This test demonstrates some cases that the piggybacking algorithm
// but conceivably might, with more modifications.  The instrumentation
// based algorithm handles them ok, though.

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

int x = 0;

typedef long long Long;

int main(void)
{
   assert(4 == sizeof(int));
   assert(8 == sizeof(Long));

   // 64-bit undefined double.
   {
      double* ptr_to_undef_double = malloc(sizeof(double));
      double  undef_double = *ptr_to_undef_double;
      fprintf(stderr, "\nUndef 1 of 3 (64-bit FP)\n");
      x += (undef_double < (double)123.45 ? 12 : 23);
   }

   // 32-bit undefined float.
   {
      float* ptr_to_undef_float = malloc(sizeof(float));
      float undef_float = *ptr_to_undef_float;
      fprintf(stderr, "\nUndef 2 of 3 (32-bit FP)\n");
      x += (undef_float < (float)234.56  ? 13 : 24);
   }

   // Stack, 32-bit, recently modified.
   // Problem here is that we don't chase backwards through loads and
   // stores.  Ie. the variable is stored after it's been modified, then
   // loaded again, so we don't see the unmodified version.
   {
      int modified_undef_stack_int;
      modified_undef_stack_int++;
      fprintf(stderr, "\nUndef 3 of 3 (int)\n");
      x += (modified_undef_stack_int == 0x1234 ? 11 : 22);
   }
   
   return x;
}
