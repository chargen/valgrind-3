#include <stdio.h>
#include <stdlib.h>

#define	I64(C)	"rcrq %%rbx\n" "rclq $" #C ",%%rax\n" "rclq %%rbx\n"
#define	I32(C)	"rcrl %%ebx\n" "rcll $" #C ",%%eax\n" "rcll %%ebx\n"
#define	I16(C)	"rcrw %%bx\n"  "rclw $" #C ",%%ax\n"  "rclw %%bx\n"
#define	I8(C)	"rcrb %%bl\n"  "rclb $" #C ",%%al\n"  "rclb %%bl\n"

#define TRY64(C)							\
	do { for(i = 0 ; C && (i * C < 2 * 65) ; i++) {			\
		printf("C=" #C ", w=64, i=%-3d a=%016lx\n",i,a);	\
		asm(I64(C) : "+a"(a), "+b"(b) : /* */);			\
	} } while (0)
#define TRY32(C)							\
	do { for(i = 0 ; C && (i * C < 2 * 33) ; i++) {			\
		printf("C=" #C ", w=32, i=%-3d a=%016lx\n",i,a);	\
		asm(I32(C) : "+a"(a), "+b"(b) : /* */);			\
	} } while (0)
#define TRY16(C)							\
	do { for(i = 0 ; C && (i * C < 2 * 17) ; i++) {			\
		printf("C=" #C ", w=16, i=%-3d a=%016lx\n",i,a);	\
		asm(I16(C) : "+a"(a), "+b"(b) : /* */);			\
	} } while (0)
#define TRY8(C)								\
	do { for(i = 0 ; C && (i * C < 2 * 9) ; i++) {			\
		printf("C=" #C ", w=8, i=%-3d a=%016lx\n",i,a);		\
		asm(I8(C) : "+a"(a), "+b"(b) : /* */);			\
	} } while (0)

int main(int argc, char * argv[])
{
	register unsigned long a, b;
	int i;
	if (argc == 2)
		a = atol(argv[1]);
	else
		a = 0x0017004200ab0cdUL;
	b = 0;

	TRY64(0); TRY64(1); TRY64(2); TRY64(3); TRY64(4); TRY64(5);
	TRY64(6); TRY64(7); TRY64(8); TRY64(9); TRY64(10); TRY64(11);
	TRY64(12); TRY64(13); TRY64(14); TRY64(15); TRY64(16); TRY64(17);
	TRY64(18); TRY64(19); TRY64(20); TRY64(21); TRY64(22); TRY64(23);
	TRY64(24); TRY64(25); TRY64(26); TRY64(27); TRY64(28); TRY64(29);
	TRY64(30); TRY64(31); TRY64(32); TRY64(33); TRY64(34); TRY64(35);
	TRY64(36); TRY64(37); TRY64(38); TRY64(39); TRY64(40); TRY64(41);
	TRY64(42); TRY64(43); TRY64(44); TRY64(45); TRY64(46); TRY64(47);
	TRY64(48); TRY64(49); TRY64(50); TRY64(51); TRY64(52); TRY64(53);
	TRY64(54); TRY64(55); TRY64(56); TRY64(57); TRY64(58); TRY64(59);
	TRY64(60); TRY64(61); TRY64(62); TRY64(63); TRY64(64); TRY64(65);
	TRY64(66); TRY64(67); TRY64(68); TRY64(69); TRY64(70); TRY64(71);
	TRY64(72); TRY64(73); TRY64(74); TRY64(75); TRY64(76); TRY64(77);
	TRY64(78); TRY64(79); TRY64(80); TRY64(81); TRY64(82); TRY64(83);
	TRY64(84); TRY64(85); TRY64(86); TRY64(87); TRY64(88); TRY64(89);
	TRY64(90); TRY64(91); TRY64(92); TRY64(93); TRY64(94); TRY64(95);
	TRY64(96); TRY64(97); TRY64(98); TRY64(99); TRY64(100);
	TRY64(101); TRY64(102); TRY64(103); TRY64(104); TRY64(105);
	TRY64(106); TRY64(107); TRY64(108); TRY64(109); TRY64(110);
	TRY64(111); TRY64(112); TRY64(113); TRY64(114); TRY64(115);
	TRY64(116); TRY64(117); TRY64(118); TRY64(119); TRY64(120);
	TRY64(121); TRY64(122); TRY64(123); TRY64(124); TRY64(125);
	TRY64(126); TRY64(127); TRY64(128);

	TRY32(0); TRY32(1); TRY32(2); TRY32(3); TRY32(4); TRY32(5);
	TRY32(6); TRY32(7); TRY32(8); TRY32(9); TRY32(10); TRY32(11);
	TRY32(12); TRY32(13); TRY32(14); TRY32(15); TRY32(16); TRY32(17);
	TRY32(18); TRY32(19); TRY32(20); TRY32(21); TRY32(22); TRY32(23);
	TRY32(24); TRY32(25); TRY32(26); TRY32(27); TRY32(28); TRY32(29);
	TRY32(30); TRY32(31); TRY32(32); TRY32(33); TRY32(34); TRY32(35);
	TRY32(36); TRY32(37); TRY32(38); TRY32(39); TRY32(40); TRY32(41);
	TRY32(42); TRY32(43); TRY32(44); TRY32(45); TRY32(46); TRY32(47);
	TRY32(48); TRY32(49); TRY32(50); TRY32(51); TRY32(52); TRY32(53);
	TRY32(54); TRY32(55); TRY32(56); TRY32(57); TRY32(58); TRY32(59);
	TRY32(60); TRY32(61); TRY32(62); TRY32(63); TRY32(64); TRY32(65);
	TRY32(66); TRY32(67); TRY32(68); TRY32(69); TRY32(70); TRY32(71);
	TRY32(72); TRY32(73); TRY32(74); TRY32(75); TRY32(76); TRY32(77);
	TRY32(78); TRY32(79); TRY32(80); TRY32(81); TRY32(82); TRY32(83);
	TRY32(84); TRY32(85); TRY32(86); TRY32(87); TRY32(88); TRY32(89);
	TRY32(90); TRY32(91); TRY32(92); TRY32(93); TRY32(94); TRY32(95);
	TRY32(96); TRY32(97); TRY32(98); TRY32(99); TRY32(100);
	TRY32(101); TRY32(102); TRY32(103); TRY32(104); TRY32(105);
	TRY32(106); TRY32(107); TRY32(108); TRY32(109); TRY32(110);
	TRY32(111); TRY32(112); TRY32(113); TRY32(114); TRY32(115);
	TRY32(116); TRY32(117); TRY32(118); TRY32(119); TRY32(120);
	TRY32(121); TRY32(122); TRY32(123); TRY32(124); TRY32(125);
	TRY32(126); TRY32(127); TRY32(128);

	TRY16(0); TRY16(1); TRY16(2); TRY16(3); TRY16(4); TRY16(5);
	TRY16(6); TRY16(7); TRY16(8); TRY16(9); TRY16(10); TRY16(11);
	TRY16(12); TRY16(13); TRY16(14); TRY16(15); TRY16(16); TRY16(17);
	TRY16(18); TRY16(19); TRY16(20); TRY16(21); TRY16(22); TRY16(23);
	TRY16(24); TRY16(25); TRY16(26); TRY16(27); TRY16(28); TRY16(29);
	TRY16(30); TRY16(31); TRY16(32); TRY16(33); TRY16(34); TRY16(35);
	TRY16(36); TRY16(37); TRY16(38); TRY16(39); TRY16(40); TRY16(41);
	TRY16(42); TRY16(43); TRY16(44); TRY16(45); TRY16(46); TRY16(47);
	TRY16(48); TRY16(49); TRY16(50); TRY16(51); TRY16(52); TRY16(53);
	TRY16(54); TRY16(55); TRY16(56); TRY16(57); TRY16(58); TRY16(59);
	TRY16(60); TRY16(61); TRY16(62); TRY16(63); TRY16(64); TRY16(65);
	TRY16(66); TRY16(67); TRY16(68); TRY16(69); TRY16(70); TRY16(71);
	TRY16(72); TRY16(73); TRY16(74); TRY16(75); TRY16(76); TRY16(77);
	TRY16(78); TRY16(79); TRY16(80); TRY16(81); TRY16(82); TRY16(83);
	TRY16(84); TRY16(85); TRY16(86); TRY16(87); TRY16(88); TRY16(89);
	TRY16(90); TRY16(91); TRY16(92); TRY16(93); TRY16(94); TRY16(95);
	TRY16(96); TRY16(97); TRY16(98); TRY16(99); TRY16(100);
	TRY16(101); TRY16(102); TRY16(103); TRY16(104); TRY16(105);
	TRY16(106); TRY16(107); TRY16(108); TRY16(109); TRY16(110);
	TRY16(111); TRY16(112); TRY16(113); TRY16(114); TRY16(115);
	TRY16(116); TRY16(117); TRY16(118); TRY16(119); TRY16(120);
	TRY16(121); TRY16(122); TRY16(123); TRY16(124); TRY16(125);
	TRY16(126); TRY16(127); TRY16(128);

	TRY8(0); TRY8(1); TRY8(2); TRY8(3); TRY8(4); TRY8(5); TRY8(6);
	TRY8(7); TRY8(8); TRY8(9); TRY8(10); TRY8(11); TRY8(12);
	TRY8(13); TRY8(14); TRY8(15); TRY8(16); TRY8(17); TRY8(18);
	TRY8(19); TRY8(20); TRY8(21); TRY8(22); TRY8(23); TRY8(24);
	TRY8(25); TRY8(26); TRY8(27); TRY8(28); TRY8(29); TRY8(30);
	TRY8(31); TRY8(32); TRY8(33); TRY8(34); TRY8(35); TRY8(36);
	TRY8(37); TRY8(38); TRY8(39); TRY8(40); TRY8(41); TRY8(42);
	TRY8(43); TRY8(44); TRY8(45); TRY8(46); TRY8(47); TRY8(48);
	TRY8(49); TRY8(50); TRY8(51); TRY8(52); TRY8(53); TRY8(54);
	TRY8(55); TRY8(56); TRY8(57); TRY8(58); TRY8(59); TRY8(60);
	TRY8(61); TRY8(62); TRY8(63); TRY8(64); TRY8(65); TRY8(66);
	TRY8(67); TRY8(68); TRY8(69); TRY8(70); TRY8(71); TRY8(72);
	TRY8(73); TRY8(74); TRY8(75); TRY8(76); TRY8(77); TRY8(78);
	TRY8(79); TRY8(80); TRY8(81); TRY8(82); TRY8(83); TRY8(84);
	TRY8(85); TRY8(86); TRY8(87); TRY8(88); TRY8(89); TRY8(90);
	TRY8(91); TRY8(92); TRY8(93); TRY8(94); TRY8(95); TRY8(96);
	TRY8(97); TRY8(98); TRY8(99); TRY8(100); TRY8(101); TRY8(102);
	TRY8(103); TRY8(104); TRY8(105); TRY8(106); TRY8(107); TRY8(108);
	TRY8(109); TRY8(110); TRY8(111); TRY8(112); TRY8(113); TRY8(114);
	TRY8(115); TRY8(116); TRY8(117); TRY8(118); TRY8(119); TRY8(120);
	TRY8(121); TRY8(122); TRY8(123); TRY8(124); TRY8(125); TRY8(126);
	TRY8(127); TRY8(128);

	return 0;
}
