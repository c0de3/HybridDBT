/*
 * riscvSimulator.h
 *
 *  Created on: 2 déc. 2016
 *      Author: simon
 */

#ifndef INCLUDES_SIMULATOR_RISCVSIMULATOR_H_
#define INCLUDES_SIMULATOR_RISCVSIMULATOR_H_

#ifndef __NIOS

#include <lib/ac_int.h>
#include <map>
#include <simulator/genericSimulator.h>
#include <string>
#include <types.h>
#include <unordered_map>

// Modelization of RISC-V pipeline
#define LOSS_INCORRECT_BRANCH 2
#define LOSS_PIPELINE_HAZARD 2
#define LOSS_PIPELINE_HAZARD_FORWARDED 1

class RiscvSimulator : public GenericSimulator {
public:
  ac_int<64, true> pc;
  uint64_t n_inst;

  // Modelization of RISC-V pipeline
  int lastWrittenRegister;
  bool lastIsLoad;

  RiscvSimulator(void) : GenericSimulator()
  {
    lastIsLoad          = false;
    lastWrittenRegister = -1;
    cycle               = 0;
  };
  int doSimulation(int nbCycles);

  void doStep();
};

#endif

#endif /* INCLUDES_SIMULATOR_RISCVSIMULATOR_H_ */
