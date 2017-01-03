#ifndef __DBTPLATEFORM
#define __DBTPLATEFORM

/********************************************************************
 * This file describe the class dbtPlatform.
 *
 * This class represent different memories that are used in the actual hardware platform. Those memories will be used by
 * hardware accelerators to store different information collected during the DBT process.
 *
 * dbtPlatform has the following types:
 *  -> vliwBinaries is a memory containing 128-bits words, each of them encoding a single VLIW syllabus. This memory is
 *  used by different accelerators to store their generated binaries but also by the VLIW as instruction memory.
 *  -> mipsBinaries is initialized with the MIPS binaries to translate
 *  -> insertions, insertions_type, insertions_src is the list of all places where firstPassTranslator had to insert an instruction to handle MIPS ISA
 *  correctly. TODO: describe how it is generated and how to use it
 *  ->
 *
 ********************************************************************/

#define MEMORY_SIZE 4096

#ifndef __NOT_USE_AC
#include <lib/ac_int.h>
class DBTPlateform
{

public:
	ac_int<128, false> vliwBinaries[MEMORY_SIZE];
	ac_int<32, false> mipsBinaries[MEMORY_SIZE];
	ac_int<32, false> insertions[MEMORY_SIZE];
	ac_int<8, false> insertions_type[MEMORY_SIZE];
	ac_int<32, false> insertions_src[MEMORY_SIZE];
	ac_int<16, true> blockBoundaries[MEMORY_SIZE];
	ac_int<16, true> procedureBoundaries[MEMORY_SIZE];
	ac_int<128, false> bytecode[256];
	ac_int<32, false> globalVariables[64];
};
#endif


#ifdef __NIOS
class DBTPlateform
{

public:
	unsigned int vliwBinaries[1024];
	unsigned int mipsBinaries[1024];
	unsigned int insertions[1024];
	char insertions_type[1024];
	unsigned int insertions_src[1024];
	short blockBoundaries[1024];
	short procedureBoundaries[1024];
	unsigned int bytecode[256];
	int globalVariables[64];
};
#endif

#ifdef __NIOS
unsigned int getInitCode(unsigned int *binaries, int start, unsigned int startAddress);
#endif

#ifdef __USE_AC
unsigned int getInitCode(ac_int<128, false> *binaries, int start, unsigned int startAddress);
#endif

#endif
