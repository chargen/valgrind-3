/*  L2 cache simulator, generated by vg_cachegen.
 *     total size    = 262144 bytes
 *     line size     = 64 bytes
 *     associativity = 8-way associative
 *
 *  This file should be #include-d into vg_cachesim.c
 */

static char L2_desc_line[] = 
    "desc: L2 cache:         262144 B, 64 B, 8-way associative\n";

static UInt L2_tags[512][8];

static void cachesim_L2_initcache(void)
{
   UInt set, way;
   for (set = 0; set < 512; set++)
      for (way = 0; way < 8; way++)
         L2_tags[set][way] = 0;
}

static __inline__ 
void cachesim_L2_doref(Addr a, UChar size, ULong *m2)
{
   register UInt set1 = ( a             >> 6) & (512-1);
   register UInt set2 = ((a + size - 1) >> 6) & (512-1);
   register UInt tag  = a >> (6 + 9);

   if (set1 == set2) {

      if (tag == L2_tags[set1][0]) {
         return;
      }
      else if (tag == L2_tags[set1][1]) {
         L2_tags[set1][1] = L2_tags[set1][0];
         L2_tags[set1][0] = tag;
         return;
      }
      else if (tag == L2_tags[set1][2]) {
         L2_tags[set1][2] = L2_tags[set1][1];
         L2_tags[set1][1] = L2_tags[set1][0];
         L2_tags[set1][0] = tag;
         return;
      }
      else if (tag == L2_tags[set1][3]) {
         L2_tags[set1][3] = L2_tags[set1][2];
         L2_tags[set1][2] = L2_tags[set1][1];
         L2_tags[set1][1] = L2_tags[set1][0];
         L2_tags[set1][0] = tag;
         return;
      }
      else if (tag == L2_tags[set1][4]) {
         L2_tags[set1][4] = L2_tags[set1][3];
         L2_tags[set1][3] = L2_tags[set1][2];
         L2_tags[set1][2] = L2_tags[set1][1];
         L2_tags[set1][1] = L2_tags[set1][0];
         L2_tags[set1][0] = tag;
         return;
      }
      else if (tag == L2_tags[set1][5]) {
         L2_tags[set1][5] = L2_tags[set1][4];
         L2_tags[set1][4] = L2_tags[set1][3];
         L2_tags[set1][3] = L2_tags[set1][2];
         L2_tags[set1][2] = L2_tags[set1][1];
         L2_tags[set1][1] = L2_tags[set1][0];
         L2_tags[set1][0] = tag;
         return;
      }
      else if (tag == L2_tags[set1][6]) {
         L2_tags[set1][6] = L2_tags[set1][5];
         L2_tags[set1][5] = L2_tags[set1][4];
         L2_tags[set1][4] = L2_tags[set1][3];
         L2_tags[set1][3] = L2_tags[set1][2];
         L2_tags[set1][2] = L2_tags[set1][1];
         L2_tags[set1][1] = L2_tags[set1][0];
         L2_tags[set1][0] = tag;
         return;
      }
      else if (tag == L2_tags[set1][7]) {
         L2_tags[set1][7] = L2_tags[set1][6];
         L2_tags[set1][6] = L2_tags[set1][5];
         L2_tags[set1][5] = L2_tags[set1][4];
         L2_tags[set1][4] = L2_tags[set1][3];
         L2_tags[set1][3] = L2_tags[set1][2];
         L2_tags[set1][2] = L2_tags[set1][1];
         L2_tags[set1][1] = L2_tags[set1][0];
         L2_tags[set1][0] = tag;
         return;
      }
      else {
         /* A miss */
         L2_tags[set1][7] = L2_tags[set1][6];
         L2_tags[set1][6] = L2_tags[set1][5];
         L2_tags[set1][5] = L2_tags[set1][4];
         L2_tags[set1][4] = L2_tags[set1][3];
         L2_tags[set1][3] = L2_tags[set1][2];
         L2_tags[set1][2] = L2_tags[set1][1];
         L2_tags[set1][1] = L2_tags[set1][0];
         L2_tags[set1][0] = tag;

         (*m2)++;
      }

   } else if ((set1 + 1) % 512 == set2) {

      Bool is_L2_miss = False;

      /* Block one */
      if (tag == L2_tags[set1][0]) {
      }
      else if (tag == L2_tags[set1][1]) {
         L2_tags[set1][1] = L2_tags[set1][0];
         L2_tags[set1][0] = tag;
      }
      else if (tag == L2_tags[set1][2]) {
         L2_tags[set1][2] = L2_tags[set1][1];
         L2_tags[set1][1] = L2_tags[set1][0];
         L2_tags[set1][0] = tag;
      }
      else if (tag == L2_tags[set1][3]) {
         L2_tags[set1][3] = L2_tags[set1][2];
         L2_tags[set1][2] = L2_tags[set1][1];
         L2_tags[set1][1] = L2_tags[set1][0];
         L2_tags[set1][0] = tag;
      }
      else if (tag == L2_tags[set1][4]) {
         L2_tags[set1][4] = L2_tags[set1][3];
         L2_tags[set1][3] = L2_tags[set1][2];
         L2_tags[set1][2] = L2_tags[set1][1];
         L2_tags[set1][1] = L2_tags[set1][0];
         L2_tags[set1][0] = tag;
      }
      else if (tag == L2_tags[set1][5]) {
         L2_tags[set1][5] = L2_tags[set1][4];
         L2_tags[set1][4] = L2_tags[set1][3];
         L2_tags[set1][3] = L2_tags[set1][2];
         L2_tags[set1][2] = L2_tags[set1][1];
         L2_tags[set1][1] = L2_tags[set1][0];
         L2_tags[set1][0] = tag;
      }
      else if (tag == L2_tags[set1][6]) {
         L2_tags[set1][6] = L2_tags[set1][5];
         L2_tags[set1][5] = L2_tags[set1][4];
         L2_tags[set1][4] = L2_tags[set1][3];
         L2_tags[set1][3] = L2_tags[set1][2];
         L2_tags[set1][2] = L2_tags[set1][1];
         L2_tags[set1][1] = L2_tags[set1][0];
         L2_tags[set1][0] = tag;
      }
      else if (tag == L2_tags[set1][7]) {
         L2_tags[set1][7] = L2_tags[set1][6];
         L2_tags[set1][6] = L2_tags[set1][5];
         L2_tags[set1][5] = L2_tags[set1][4];
         L2_tags[set1][4] = L2_tags[set1][3];
         L2_tags[set1][3] = L2_tags[set1][2];
         L2_tags[set1][2] = L2_tags[set1][1];
         L2_tags[set1][1] = L2_tags[set1][0];
         L2_tags[set1][0] = tag;
      }
      else {
         /* A miss */
         L2_tags[set1][7] = L2_tags[set1][6];
         L2_tags[set1][6] = L2_tags[set1][5];
         L2_tags[set1][5] = L2_tags[set1][4];
         L2_tags[set1][4] = L2_tags[set1][3];
         L2_tags[set1][3] = L2_tags[set1][2];
         L2_tags[set1][2] = L2_tags[set1][1];
         L2_tags[set1][1] = L2_tags[set1][0];
         L2_tags[set1][0] = tag;

         is_L2_miss = True;
      }

      /* Block two */
      if (tag == L2_tags[set2][0]) {
      }
      else if (tag == L2_tags[set2][1]) {
         L2_tags[set2][1] = L2_tags[set2][0];
         L2_tags[set2][0] = tag;
      }
      else if (tag == L2_tags[set2][2]) {
         L2_tags[set2][2] = L2_tags[set2][1];
         L2_tags[set2][1] = L2_tags[set2][0];
         L2_tags[set2][0] = tag;
      }
      else if (tag == L2_tags[set2][3]) {
         L2_tags[set2][3] = L2_tags[set2][2];
         L2_tags[set2][2] = L2_tags[set2][1];
         L2_tags[set2][1] = L2_tags[set2][0];
         L2_tags[set2][0] = tag;
      }
      else if (tag == L2_tags[set2][4]) {
         L2_tags[set2][4] = L2_tags[set2][3];
         L2_tags[set2][3] = L2_tags[set2][2];
         L2_tags[set2][2] = L2_tags[set2][1];
         L2_tags[set2][1] = L2_tags[set2][0];
         L2_tags[set2][0] = tag;
      }
      else if (tag == L2_tags[set2][5]) {
         L2_tags[set2][5] = L2_tags[set2][4];
         L2_tags[set2][4] = L2_tags[set2][3];
         L2_tags[set2][3] = L2_tags[set2][2];
         L2_tags[set2][2] = L2_tags[set2][1];
         L2_tags[set2][1] = L2_tags[set2][0];
         L2_tags[set2][0] = tag;
      }
      else if (tag == L2_tags[set2][6]) {
         L2_tags[set2][6] = L2_tags[set2][5];
         L2_tags[set2][5] = L2_tags[set2][4];
         L2_tags[set2][4] = L2_tags[set2][3];
         L2_tags[set2][3] = L2_tags[set2][2];
         L2_tags[set2][2] = L2_tags[set2][1];
         L2_tags[set2][1] = L2_tags[set2][0];
         L2_tags[set2][0] = tag;
      }
      else if (tag == L2_tags[set2][7]) {
         L2_tags[set2][7] = L2_tags[set2][6];
         L2_tags[set2][6] = L2_tags[set2][5];
         L2_tags[set2][5] = L2_tags[set2][4];
         L2_tags[set2][4] = L2_tags[set2][3];
         L2_tags[set2][3] = L2_tags[set2][2];
         L2_tags[set2][2] = L2_tags[set2][1];
         L2_tags[set2][1] = L2_tags[set2][0];
         L2_tags[set2][0] = tag;
      }
      else {
         /* A miss */
         L2_tags[set2][7] = L2_tags[set2][6];
         L2_tags[set2][6] = L2_tags[set2][5];
         L2_tags[set2][5] = L2_tags[set2][4];
         L2_tags[set2][4] = L2_tags[set2][3];
         L2_tags[set2][3] = L2_tags[set2][2];
         L2_tags[set2][2] = L2_tags[set2][1];
         L2_tags[set2][1] = L2_tags[set2][0];
         L2_tags[set2][0] = tag;

         is_L2_miss = True;
      }

      /* Miss treatment */
      if (is_L2_miss) {
         (*m2)++;
      }

   } else {
      VG_(printf)("\nERROR: Data item 0x%x of size %u bytes is in two non-adjacent\n", a, size);
      VG_(printf)("sets %d and %d.\n", set1, set2);
      VG_(panic)("L2 cache set mismatch");
   }
}
