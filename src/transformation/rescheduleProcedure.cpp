/*
 * rescheduleProcedure.cpp
 *
 *  Created on: 30 mai 2017
 *      Author: Simon Rokicki
 */

#include <dbt/dbtPlateform.h>
#include <isa/irISA.h>
#include <transformation/irScheduler.h>
#include <lib/endianness.h>
#include <transformation/reconfigureVLIW.h>
#include <transformation/rescheduleProcedure.h>
#include <lib/log.h>

/******************************************************************************************
 ******************************  Reschedule Procedure
 ******************************************************************************************
 * This transformation will take as argument a procedure and will schedule and place all
 * blocks. It will also modify the branch parameters so that they target the new block locations
 * and will insert some jump in the old code to link toward the new location of binaries.
 *
 * Note that this transformation MODIFY the vliw code memory but DO NOT NEED THE VLIW TO BE STOPPED.
 * Indeed, the code modification is done at a new location and the linkage is only done at the end
 * of the transformation. At any time, if the vliw is executing the code modified, it should not affect
 * the normal execution.
 *
 * Arguments are :
 * 	-> platform is the DBTPlatform containing all global memories
 * 	-> procedure is the procedure that we want to schedule
 * 	-> writePlace is the destination when we will generate the new binaries
 *
 * 	Transformation returns the sum of writePlace and the size of the generated binaries.
 ******************************************************************************************/

int rescheduleProcedure(DBTPlateform *platform, IRProcedure *procedure,int writePlace){

	IRProcedure *scheduledProc = rescheduleProcedure_schedule(platform, procedure, writePlace);
	return rescheduleProcedure_commit(platform, procedure, writePlace, scheduledProc);
}

IRProcedure* rescheduleProcedure_schedule(DBTPlateform *platform, IRProcedure *procedure,int writePlace){

	char issueWidth = getIssueWidth(procedure->configuration);
	char incrementInBinaries = (getIssueWidth(procedure->configuration)>4) ? 2 : 1;

	IRBlock **blocks = (IRBlock **) malloc(procedure->nbBlock * sizeof(IRBlock*));
	for (int oneBlock = 0; oneBlock<procedure->nbBlock; oneBlock++){
		blocks[oneBlock] = new IRBlock(-1,-1,-1);
	}
	IRProcedure *result = new IRProcedure(blocks[0], procedure->nbBlock);
	result->blocks = blocks;
	result->configuration = procedure->configuration;

	/******************************************************************************************
	 ******************************  Scheduling of all blocks
	 ******************************************************************************************
	 * For each block of the procedure, we will do the instruction scheduling and keep track of their
	 * previous start address as well as the place where the jump instruction (if any) as been scheduled.
	 ******************************************************************************************/

	for (int oneBlock = 0; oneBlock<procedure->nbBlock; oneBlock++){
		IRBlock *block = procedure->blocks[oneBlock];

		bool isCallBlock = false;
		bool isReturnBlock = false;

		if (block->nbJumps > 0){
			char opcode = getOpcode(block->instructions, block->jumpIds[block->nbJumps-1]);
			isCallBlock = opcode == VEX_CALL || opcode == VEX_CALLR;
			isReturnBlock = opcode == VEX_GOTOR;

		}


		//We move instructions into bytecode memory
		for (int oneBytecodeInstr = 0; oneBytecodeInstr<block->nbInstr; oneBytecodeInstr++){
			writeInt(platform->bytecode, 16*oneBytecodeInstr + 0, block->instructions[4*oneBytecodeInstr + 0]);
			writeInt(platform->bytecode, 16*oneBytecodeInstr + 4, block->instructions[4*oneBytecodeInstr + 1]);
			writeInt(platform->bytecode, 16*oneBytecodeInstr + 8, block->instructions[4*oneBytecodeInstr + 2]);
			writeInt(platform->bytecode, 16*oneBytecodeInstr + 12, block->instructions[4*oneBytecodeInstr + 3]);
		}

		//We initialize other memories
		for (int oneFreeRegister = 34; oneFreeRegister<63; oneFreeRegister++)
			platform->freeRegisters[oneFreeRegister-34] = oneFreeRegister;

		for (int onePlaceOfRegister = 0; onePlaceOfRegister<64; onePlaceOfRegister++)
			platform->placeOfRegisters[256+onePlaceOfRegister] = onePlaceOfRegister;

		//same for FP registers
		for (int onePlaceOfRegister = 0; onePlaceOfRegister<64; onePlaceOfRegister++)
			platform->placeOfRegisters[256+64+onePlaceOfRegister] = onePlaceOfRegister;


		//This is only for debug
		if (platform->debugLevel > 1 || 1){
			Log::printf(LOG_SCHEDULE_PROC,"Block %x:\n", block->sourceStartAddress);
			for (int i=0; i<block->nbInstr; i++)
				Log::printf(LOG_SCHEDULE_PROC,"%s ", printBytecodeInstruction(i, readInt(platform->bytecode, i*16+0), readInt(platform->bytecode, i*16+4), readInt(platform->bytecode, i*16+8), readInt(platform->bytecode, i*16+12)).c_str());
		}


		//We call the scheduler
		int binaSize = irScheduler(platform, 1,block->nbInstr, writePlace, 29, procedure->configuration);

		result->blocks[oneBlock]->vliwStartAddress = writePlace;
		result->blocks[oneBlock]->vliwEndAddress = writePlace + binaSize;

		//We copy and modify the jump ids and their location
		result->blocks[oneBlock]->jumpIds = (unsigned char*) malloc(block->nbJumps * sizeof(unsigned char));
		result->blocks[oneBlock]->jumpPlaces = (unsigned int*) malloc(block->nbJumps * sizeof(unsigned int));
		result->blocks[oneBlock]->nbJumps = block->nbJumps;

		for (int oneJump=0; oneJump<block->nbJumps; oneJump++){
			result->blocks[oneBlock]->jumpIds[oneJump] = block->jumpIds[oneJump];
#ifdef IR_SUCC
			result->blocks[oneBlock]->jumpPlaces[oneJump] = ((int) platform->placeOfInstr[block->jumpIds[oneJump]])+writePlace;
#else
			result->blocks[oneBlock]->jumpPlaces[oneJump] = incrementInBinaries*((int) platform->placeOfInstr[block->jumpIds[oneJump]])+writePlace;
#endif

		}

		if (1){
			for (int i=result->blocks[oneBlock]->vliwStartAddress;i<result->blocks[oneBlock]->vliwEndAddress;i++){
				Log::printf(LOG_SCHEDULE_PROC,"%d ", i);

				Log::printf(LOG_SCHEDULE_PROC,"%s ", printDecodedInstr(platform->vliwBinaries[i].slc<32>(0)).c_str());
				Log::printf(LOG_SCHEDULE_PROC,"%s ", printDecodedInstr(platform->vliwBinaries[i].slc<32>(32)).c_str());
				Log::printf(LOG_SCHEDULE_PROC,"%s ", printDecodedInstr(platform->vliwBinaries[i].slc<32>(64)).c_str());
				Log::printf(LOG_SCHEDULE_PROC,"%s ", printDecodedInstr(platform->vliwBinaries[i].slc<32>(96)).c_str());


				if (issueWidth>4){
					Log::printf(LOG_SCHEDULE_PROC,"%s ", printDecodedInstr(platform->vliwBinaries[i+1].slc<32>(0)).c_str());
					Log::printf(LOG_SCHEDULE_PROC,"%s ", printDecodedInstr(platform->vliwBinaries[i+1].slc<32>(32)).c_str());
					Log::printf(LOG_SCHEDULE_PROC,"%s ", printDecodedInstr(platform->vliwBinaries[i+1].slc<32>(64)).c_str());
					Log::printf(LOG_SCHEDULE_PROC,"%s ", printDecodedInstr(platform->vliwBinaries[i+1].slc<32>(96)).c_str());
					i++;
				}


				Log::printf(LOG_SCHEDULE_PROC,"\n");
			}
		}
		if ((isReturnBlock || isCallBlock) && readInt(platform->vliwBinaries, 16*result->blocks[oneBlock]->vliwEndAddress - 16*incrementInBinaries) != 0){

			if (readInt(platform->vliwBinaries, 16*result->blocks[oneBlock]->vliwEndAddress - 16*incrementInBinaries).slc<6>(20) == 33){
				writeInt(platform->vliwBinaries, 16*result->blocks[oneBlock]->vliwEndAddress - 2*16*incrementInBinaries, 0);
				writeInt(platform->vliwBinaries, 16*result->blocks[oneBlock]->vliwEndAddress, readInt(platform->vliwBinaries, 16*result->blocks[oneBlock]->vliwEndAddress - 2*16*incrementInBinaries));


			}
			else{
				//We need room for the reconf instruction, we invert the two last
				unsigned int tempInstr = readInt(platform->vliwBinaries, 16*result->blocks[oneBlock]->vliwEndAddress - 16*incrementInBinaries);
				writeInt(platform->vliwBinaries, 16*result->blocks[oneBlock]->vliwEndAddress - 16*incrementInBinaries, readInt(platform->vliwBinaries, 16*result->blocks[oneBlock]->vliwEndAddress - 2*16*incrementInBinaries));
				writeInt(platform->vliwBinaries, 16*result->blocks[oneBlock]->vliwEndAddress - 16*incrementInBinaries*2, tempInstr);
				result->blocks[oneBlock]->jumpPlaces[result->blocks[oneBlock]->nbJumps-1]++;
				result->blocks[oneBlock]->vliwEndAddress += incrementInBinaries;
				binaSize += incrementInBinaries;
			}

		}



		if (isCallBlock){
			writeInt(platform->vliwBinaries, 16*(writePlace+binaSize), 0);
			writeInt(platform->vliwBinaries, 16*(writePlace+binaSize)+4, 0);
			writeInt(platform->vliwBinaries, 16*(writePlace+binaSize)+8, 0);
			writeInt(platform->vliwBinaries, 16*(writePlace+binaSize)+12, 0);
			binaSize += 1;

			if (getIssueWidth(platform->vliwInitialConfiguration)>4){
				writeInt(platform->vliwBinaries, 16*(writePlace + binaSize+1), 0);
				writeInt(platform->vliwBinaries, 16*(writePlace + binaSize+1)+4, 0);
				writeInt(platform->vliwBinaries, 16*(writePlace + binaSize+1)+8, 0);
				writeInt(platform->vliwBinaries, 16*(writePlace + binaSize+1)+12, 0);
				binaSize += 1;

			}
		}

		writePlace+=binaSize;

	}

	return result;

}

int rescheduleProcedure_commit(DBTPlateform *platform, IRProcedure *procedure,int writePlace, IRProcedure *scheduledProc){

	char issueWidth = getIssueWidth(procedure->configuration);
	char incrementInBinaries = (getIssueWidth(procedure->configuration)>4) ? 2 : 1;
	int *oldBlockStarts = (int*) malloc(procedure->nbBlock * sizeof(int));
	int originalWritePlace = writePlace;


	for (int oneBlock = 0; oneBlock<procedure->nbBlock; oneBlock++){
		oldBlockStarts[oneBlock] = procedure->blocks[oneBlock]->vliwStartAddress;


		procedure->blocks[oneBlock]->vliwStartAddress = scheduledProc->blocks[oneBlock]->vliwStartAddress;
		procedure->blocks[oneBlock]->vliwEndAddress = scheduledProc->blocks[oneBlock]->vliwEndAddress;

		for (int oneJump = 0; oneJump<procedure->blocks[oneBlock]->nbJumps; oneJump++)
			procedure->blocks[oneBlock]->jumpPlaces[oneJump] = scheduledProc->blocks[oneBlock]->jumpPlaces[oneJump];


		if (procedure->blocks[oneBlock]->vliwEndAddress > writePlace)
			writePlace = procedure->blocks[oneBlock]->vliwEndAddress;
	}



	/******************************************************************************************
	 ******************************  Jump correction
	 ******************************************************************************************
	 * Now that all blocks have been scheduled correctly, we can go through all jump instruction and
	 * modify their destination to fit with the new block position.
	 ******************************************************************************************/

	for (int oneBlock = 0; oneBlock<procedure->nbBlock; oneBlock++){
		IRBlock *block = procedure->blocks[oneBlock];

		for (int oneJump = 0; oneJump<block->nbJumps; oneJump++){
			char jumpOpcode = getOpcode(block->instructions, block->jumpIds[oneJump]);

			if (jumpOpcode == VEX_BR || jumpOpcode == VEX_BRF){
				//Conditional block (br)
				int offset = (block->successors[oneJump]->vliwStartAddress - block->jumpPlaces[oneJump]);
				unsigned int oldJump = readInt(platform->vliwBinaries, 16*block->jumpPlaces[oneJump]);
				writeInt(platform->vliwBinaries, 16*block->jumpPlaces[oneJump], (oldJump & 0xfc00007f) | ((offset & 0x7ffff) << 7));

			}
			else if (jumpOpcode != VEX_CALL && jumpOpcode != VEX_CALLR && jumpOpcode != VEX_GOTOR){

				fprintf(stderr, "block has %d successor, %d jumps and we are accessing succ %d... place is %d; id %d opcode %x\n", block->nbSucc, block->nbJumps, oneJump, block->jumpPlaces[oneJump], block->jumpIds[oneJump], jumpOpcode	);
				int dest = block->successors[oneJump]->vliwStartAddress;
				unsigned int oldJump = readInt(platform->vliwBinaries, 16*block->jumpPlaces[oneJump]);
				writeInt(platform->vliwBinaries, 16*block->jumpPlaces[oneJump], (oldJump & 0xfc00007f) | ((dest & 0x7ffff) << 7));


			}

		}

	}

	/******************************************************************************************
	 ******************************  Link to the newly generated code
	 ******************************************************************************************
	 * For each basic block in the procedure, we will insert jump instructions at the previous start
	 * address targeting to the new start address. This way, the execution will simply switch toward the newly
	 * generated binaries.
	 ******************************************************************************************/
	fprintf(stderr, "previous conf was %d\n", procedure->previousConfiguration	);

	for (int oneBlock = 0; oneBlock<procedure->nbBlock; oneBlock++){
		IRBlock *block = procedure->blocks[oneBlock];
		int originalEntry = oldBlockStarts[oneBlock];





		if (platform->vexSimulator->PC == originalEntry || platform->vexSimulator->PC == originalEntry+1 ||  platform->vexSimulator->PC == block->oldVliwStartAddress ||  platform->vexSimulator->PC == block->oldVliwStartAddress+1)
			platform->vexSimulator->doStep(2);
		fprintf(stderr, "previous conf was %d\n", procedure->previousConfiguration	);


		if (getIssueWidth(procedure->previousConfiguration) <= 4){
			fprintf(stderr, "previous conf was lower then 4\n");
			writeInt(platform->vliwBinaries, 16*originalEntry+0, assembleIInstruction(VEX_GOTO, block->vliwStartAddress, 0));
			writeInt(platform->vliwBinaries, 16*originalEntry+4, 0);
			writeInt(platform->vliwBinaries, 16*originalEntry+8, 0);
			writeInt(platform->vliwBinaries, 16*originalEntry+12, 0);
			writeInt(platform->vliwBinaries, 16*originalEntry+16, getReconfigurationInstruction(procedure->configuration));
			writeInt(platform->vliwBinaries, 16*originalEntry+20, 0);
			writeInt(platform->vliwBinaries, 16*originalEntry+24, 0);
			writeInt(platform->vliwBinaries, 16*originalEntry+28, 0);



		}
		else{
			fprintf(stderr, "previous conf was greater then 4\n");

			writeInt(platform->vliwBinaries, 16*originalEntry+0, assembleIInstruction(VEX_GOTO, block->vliwStartAddress, 0));
			writeInt(platform->vliwBinaries, 16*originalEntry+4, 0);
			writeInt(platform->vliwBinaries, 16*originalEntry+8, 0);
			writeInt(platform->vliwBinaries, 16*originalEntry+12, 0);
			writeInt(platform->vliwBinaries, 16*originalEntry+16, 0);
			writeInt(platform->vliwBinaries, 16*originalEntry+20, 0);
			writeInt(platform->vliwBinaries, 16*originalEntry+24, 0);
			writeInt(platform->vliwBinaries, 16*originalEntry+28, 0);
			writeInt(platform->vliwBinaries, 16*originalEntry+32+0, getReconfigurationInstruction(procedure->configuration));
			writeInt(platform->vliwBinaries, 16*originalEntry+32+4, 0);
			writeInt(platform->vliwBinaries, 16*originalEntry+32+8, 0);
			writeInt(platform->vliwBinaries, 16*originalEntry+32+12, 0);
			writeInt(platform->vliwBinaries, 16*originalEntry+32+16, 0);
			writeInt(platform->vliwBinaries, 16*originalEntry+32+20, 0);
			writeInt(platform->vliwBinaries, 16*originalEntry+32+24, 0);
			writeInt(platform->vliwBinaries, 16*originalEntry+32+28, 0);



		}

		if (getIssueWidth(platform->vliwInitialConfiguration) > 4){
			writeInt(platform->vliwBinaries, 16*block->oldVliwStartAddress+0, assembleIInstruction(VEX_GOTO, block->vliwStartAddress, 0));
			writeInt(platform->vliwBinaries, 16*block->oldVliwStartAddress+32, getReconfigurationInstruction(procedure->configuration));
		}
		else{
			writeInt(platform->vliwBinaries, 16*block->oldVliwStartAddress+0, assembleIInstruction(VEX_GOTO, block->vliwStartAddress, 0));
			writeInt(platform->vliwBinaries, 16*block->oldVliwStartAddress+16, getReconfigurationInstruction(procedure->configuration));
		}


		bool isReturnBlock = false;
		bool isCallBlock = false;
		if (block->nbJumps > 0){
			char opcode = getOpcode(block->instructions, block->jumpIds[block->nbJumps-1]);
			isCallBlock = opcode == VEX_CALL || opcode == VEX_CALLR;
			isReturnBlock = opcode == VEX_GOTOR;
		}

		if (isReturnBlock){
			if (block->nbJumps == 1){
				if (readInt(platform->vliwBinaries, 16*block->jumpPlaces[block->nbJumps-1] +16*incrementInBinaries) == 0)
					writeInt(platform->vliwBinaries, 16*block->jumpPlaces[block->nbJumps-1] +16*incrementInBinaries, getReconfigurationInstruction(platform->vliwInitialConfiguration));
				else{
					Log::printf(LOG_ERROR,"Failing when inserting reconfs at the end of a procedure...\nExiting...");
					exit(-1);
				}
			}
			else{
				if (readInt(platform->vliwBinaries, 16*block->vliwEndAddress -16*incrementInBinaries) == 0)
					writeInt(platform->vliwBinaries, 16*block->vliwEndAddress -16*incrementInBinaries, getReconfigurationInstruction(platform->vliwInitialConfiguration));
				else{
					Log::printf(LOG_ERROR,"Failing when inserting reconfs at the end of a procedure...\nExiting...");
					exit(-1);
				}
			}

		}



		if (isCallBlock){
			char offsetSecondLine, offsetFirstLine;

			if (getIssueWidth(platform->vliwInitialConfiguration) > 4){
				offsetSecondLine = 2*16;
			}
			else{
				offsetSecondLine = 16;
			}

			if (issueWidth>4){
				offsetFirstLine = 2*16;
			}
			else{
				offsetFirstLine = 16;
			}

			writeInt(platform->vliwBinaries, 16*block->vliwEndAddress-offsetFirstLine, getReconfigurationInstruction(platform->vliwInitialConfiguration));
			writeInt(platform->vliwBinaries, 16*block->vliwEndAddress, getReconfigurationInstruction(procedure->configuration));

		}

	}

	//*************************************************************************
	//This is only for debug
	for (int i=originalWritePlace;i<writePlace;i++){
		Log::printf(LOG_SCHEDULE_PROC,"%d ", i);

		Log::printf(LOG_SCHEDULE_PROC,"%s ", printDecodedInstr(platform->vliwBinaries[i].slc<32>(0)).c_str());
		Log::printf(LOG_SCHEDULE_PROC,"%s ", printDecodedInstr(platform->vliwBinaries[i].slc<32>(32)).c_str());
		Log::printf(LOG_SCHEDULE_PROC,"%s ", printDecodedInstr(platform->vliwBinaries[i].slc<32>(64)).c_str());
		Log::printf(LOG_SCHEDULE_PROC,"%s ", printDecodedInstr(platform->vliwBinaries[i].slc<32>(96)).c_str());


		if (issueWidth>4){
			Log::printf(LOG_SCHEDULE_PROC,"%s ", printDecodedInstr(platform->vliwBinaries[i+1].slc<32>(0)).c_str());
			Log::printf(LOG_SCHEDULE_PROC,"%s ", printDecodedInstr(platform->vliwBinaries[i+1].slc<32>(32)).c_str());
			Log::printf(LOG_SCHEDULE_PROC,"%s ", printDecodedInstr(platform->vliwBinaries[i+1].slc<32>(64)).c_str());
			Log::printf(LOG_SCHEDULE_PROC,"%s ", printDecodedInstr(platform->vliwBinaries[i+1].slc<32>(96)).c_str());
			i++;
		}


		Log::printf(LOG_SCHEDULE_PROC,"\n");
	}


	for (int i=originalWritePlace;i<writePlace;i++){


		Log::printf(LOG_SCHEDULE_PROC,"0x%x, ", platform->vliwBinaries[i].slc<32>(0));
		Log::printf(LOG_SCHEDULE_PROC,"0x%x, ", platform->vliwBinaries[i].slc<32>(32));
		Log::printf(LOG_SCHEDULE_PROC,"0x%x, ", platform->vliwBinaries[i].slc<32>(64));
		Log::printf(LOG_SCHEDULE_PROC,"0x%x, ", platform->vliwBinaries[i].slc<32>(96));

		if (issueWidth>4){
			Log::printf(LOG_SCHEDULE_PROC,"0x%x, ", platform->vliwBinaries[i+1].slc<32>(0));
			Log::printf(LOG_SCHEDULE_PROC,"0x%x, ", platform->vliwBinaries[i+1].slc<32>(32));
			Log::printf(LOG_SCHEDULE_PROC,"0x%x, ", platform->vliwBinaries[i+1].slc<32>(64));
			Log::printf(LOG_SCHEDULE_PROC,"0X%x, ", platform->vliwBinaries[i+1].slc<32>(96));
			i++;
		}
		Log::printf(LOG_SCHEDULE_PROC,"\n");
	}
	//*************************************************************************



	return writePlace;

}

