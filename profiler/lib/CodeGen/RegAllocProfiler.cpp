#include "llvm/ADT/IndexedMap.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineOptimizationRemarkEmitter.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegAllocRegistry.h"
#include "llvm/CodeGen/RegisterClassInfo.h"
#include "llvm/CodeGen/SlotIndexes.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/CodeGen/VirtRegMap.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/Pass.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/RegAllocProfiler.h"
#include <set> 
#include <cassert> 
#include <fstream>

using namespace llvm;

// TODO: get rid of originalVRegSet constructor and compute original VRegSet manual
RegAllocProfiler::RegAllocProfiler (MachineFunction* MF, 
                                    const TargetRegisterInfo* TRI,
                                    VirtRegMap* VRM,
                                    MachineRegisterInfo* MRI,
                                    std::set<Register>* splitMarkedVRegs)
                                    :MF(MF), 
                                    TRI(TRI), 
                                    VRM(VRM), 
                                    MRI(MRI),
                                    VRegInfo(MRI->getVirtRegInfo()),
                                    regMap(VRM->getVirtRegMap()), 
                                    splitMarkedVRegs(splitMarkedVRegs)
                                    {
                                      nominalVirtRegs = MRI->getNumVirtRegs(); 
                                      numUsedVirtRegs = allocatedVirtRegs = numSpilledVirtRegs = 0;
                                    }

bool RegAllocProfiler::isUsedInFunction(Register reg) {
  assert (reg.isVirtual() && "Not a virtual register!");
  return (!MRI->use_empty(reg)); 
} 

bool RegAllocProfiler::isSpilled(Register reg) {

 // sentinel value used by VRM to indicate a non-stack spilled virtReg
 unsigned NO_STACK_SLOT = (1L<<30) -1;  

 if (VRM->getStackSlot(reg) != NO_STACK_SLOT)
   return true; 

 return false; 

} 

bool RegAllocProfiler::isMappedToPhysReg(Register reg) { 
  assert (reg.isVirtual() && "Not a virtual register!");
  Register vrmMapping = regMap[reg]; 
 
  // VirtReg can only be mapped to a physReg if it is in the physical register namespace and is used in the function
  // TODO: potential bug here, Stack slot mappings from VRM are in the same numberspace as phys reg assignments
  // so we need to check registers that are also in the stack slot
  // Update: Added logic to check spillling
  //if (Register::isPhysicalRegister(vrmMapping) && RegAllocProfiler::isUsedInFunction(reg) && !RegAllocProfiler::isSpilled(reg) )
  if (VRM->isAssignedReg(reg))
    return true;

  return false; 
} 

// Gets the numberspace mapping of the virtReg (if it has a physReg mapping)
unsigned RegAllocProfiler::getPhysRegMapping (Register reg) {
  assert(reg.isVirtual() && "Not a virtual register!");
  assert(RegAllocProfiler::isMappedToPhysReg(reg) && "Not mapped to physReg!");

  return regMap[reg]; 
}

// Gets the ASM name of the mapped phys reg
StringRef RegAllocProfiler::getPhysRegName(Register reg) {
  assert(RegAllocProfiler::isMappedToPhysReg(reg) && "Not mapped to physreg!");

  // get the physReg number that the the virtReg is mapped to 
  auto physReg = regMap[reg];
  return TRI->getRegAsmName(physReg);
}

// Dumps all the MachineInstructions associated with a virtReg
void RegAllocProfiler::dumpVirtRegInstructions(Register reg) {

  if (RegAllocProfiler::isUsedInFunction(reg)) {
    auto reg_instr_iterator = MRI->use_instructions(reg);
    for (MachineInstr& I : reg_instr_iterator) {
      errs() << I << '\n'; 
    } 
  }
} 

bool RegAllocProfiler::getRegAllocation(Register reg) {
  return origVRegInfo[reg];
} 

std::string RegAllocProfiler::getRegClassName(Register reg) {
  
  // Check the VRegInfo table entry to get RegClass info
  // The first element in the std::pair<> returned by VRegInfo
  // is an LLVM PointerUnion object that reprsents a pointer union
  // between a TargetRegisterClass* and RegisterBank* objects
  auto vRegInfoPointer = VRegInfo[reg].first; 

  // check if it's a TargetRegClass pointer, if it is return the regclass name
  if (vRegInfoPointer.is<const TargetRegisterClass*>()) {
    auto* regClass = vRegInfoPointer.get<const TargetRegisterClass*>(); 
    return std::string(TRI->getRegClassName(regClass));
  } 

   // I'll add register bank handling at some point, seems like default 
   // is always TargetRegisterClass objects though
   return "";
} 

// Goes through the DS holding all the virtReg mappings and dumps everything
void RegAllocProfiler::dump() {

  for (auto const& pair : regClassMap) {
    errs() << "***********  Register Class: " << pair.first << " *********" <<'\n';
    errs() << '\n';

    // dump all reg names and mapped virtregs
    for (auto const& mappings: pair.second.nmap) {
      errs() << mappings.first << ": " << '\n'; 

      // convert vReg back to index for easy reading
      for (auto vReg : mappings.second){
        errs() << "     ";
        errs() << Register::virtReg2Index(vReg) << '\n';
      }

    }
    errs() << '\n';
    errs() << "*******************************************" << '\n';
  } 
}

// Prints all the calculated profiler stats
void RegAllocProfiler::dumpProfilerStats() {

  errs() << "***************** RegAllocProfiler stats for Function: " << MF->getName() << "****************" << '\n';

  errs() << "Nominal VirtRegs: " << nominalVirtRegs << '\n';
  errs() << "VirtRegs used in Function: " << numUsedVirtRegs << '\n';
  errs() << "VirtRegs allocated to PhysRegs: " << allocatedVirtRegs << '\n';
  // HKHAJ - added updated print for spilling
  errs() << "VirtRegs spilled: " << numSpilledVirtRegs << '\n';

  errs() << '\n';
  errs() << '\n';

  errs() << "Breakdown per register class:" << '\n';
  for (auto const& pair : regClassData) {
    errs() << "Register class: " << pair.first << '\n';
    errs() << "Number of unique registers allocated to this class: " << pair.second.first << '\n';
    errs() << "Total number of virtRegs allocated to this class: " << pair.second.second << '\n';
    errs() << '\n';
  }

  errs() << '\n';
  errs() << "*************************************************************************" << '\n';
}

void RegAllocProfiler::dumpProfStatsToFile(std::string fname) {
  std::ofstream f; 
  f.open(fname,std::fstream::app); 
  f << '\n';
  f << "FunctionName " <<(std::string)MF->getName() << '\n';
  f << "numVirtRegs " << numUsedVirtRegs << '\n'; 
  f << "allocatedVirtRegs " << allocatedVirtRegs << '\n';
  f << "spilledVirtRegs " << numSpilledVirtRegs << '\n';
  f << "endfunctionstats" << '\n';
  f << '\n';
  f.close();
 
} 

// 10/31 -- Added method to dump all mappings for origVRegSet
void RegAllocProfiler::dumpOrigVRegMappings() {
  errs() << "***************VREG MAPPINGS********************" << '\n';

  for (auto const& pair : origVRegInfo) {
    errs() << Register::virtReg2Index(pair.first) << " : " << pair.second << '\n';
  } 
} 

void RegAllocProfiler::writeDebugStats (MachineFunction& MF) {
 
  errs() << "********* RAGreedy Debug Statistics for Function: " << MF.getName() << " *********" << '\n';
  errs() << '\n';

  errs() << " *** Summary  *** " << '\n';
  errs() << "Number of enqueued variables: " << originalVRegSet.size() << '\n';
  errs() << "Allocated to phys regs: " << allocatedVRegs.size() << '\n';
  errs() << "Spilled: " << spilledVRegs.size() << '\n';
  errs() << '\n';

  errs() << "Allocated Variables" << '\n';
  for (auto reg : allocatedVRegs) {
    errs() << Register::virtReg2Index(reg) << ',';
  } 
  errs() << '\n';

  errs() << "Spilled Variables" << '\n';
  for (auto reg : allocatedVRegs) {
    errs() << Register::virtReg2Index(reg) << ',';
  } 
  errs() << '\n';

  errs() << "Variable Data" << '\n';
  for (auto const& pair : vRegClassData) {
    errs() << "Num " << pair.first << " Variables: " << pair.second.size();
  } 
  errs() << "*********" << '\n';

  errs() << "Allocations per class" << '\n';
  for (auto const& pair : regClassData) {
    errs() << "Class: " << pair.first << " : " << pair.second.second << '\n';
  }
  errs() << "*********" << '\n';


  errs() << "Variable states + instrs" << '\n';
  for (auto reg : originalVRegSet) {

    errs() << "REGISTER: " << Register::virtReg2Index(reg) << '\n';
    errs() << "Allocation status: " << origVRegInfo[reg] << '\n';

    if (origVRegInfo[reg])
      errs() << "Allocated to: " << getPhysRegName(reg) << '\n';

    errs() << "Machine Instructions: " << '\n';
    dumpVirtRegInstructions(reg);

  } 

  errs() << " Variable Class Map Dump " << '\n';
  dump();

  
}


void RegAllocProfiler::init() {
  getOriginalVRegs();
} 

void RegAllocProfiler::computeStats() {
  populateRegisterClassMap();
  calculateProfilerStats();
} 

/********************* Private Methods *******************************/

// Populates the map with each physical reg name and it's mapped virtregs
// 10/31 Update: Updated this method to explicitly check for spilling. If a spilled 
// vReg is found, it wil updated the spill counter and will update the vReg's entry in
// origVRegInfo with the corresponding boolean value (1 for physReg mapping, 0 for spill)
void RegAllocProfiler::populateRegisterClassMap() {

  // Go through every virtual register, and check if it's actually used or not
  // If it is, get the register class name and add it to our register class map
  // Since we have a nested map, we need to add the register name and the virtual register index to our struct


  for (auto virtReg : originalVRegSet) {

    const bool isInSplitSet = splitMarkedVRegs->find(virtReg) != splitMarkedVRegs->end();

    /*
    errs() << "Num split vregs: " << splitMarkedVRegs->size() << '\n';

    for (auto vreg : *splitMarkedVRegs) {
      errs() << "spilled reg: " << Register::virtReg2Index(vreg) << '\n';
    } 

    if (isInSplitSet)
      errs() << "REGISTER: " << Register::virtReg2Index(virtReg) << "is spilled!" << '\n';
      */

    // 11/10 - added checking for split-marked vRegs
    if (isSpilled(virtReg) || isInSplitSet) {
      numSpilledVirtRegs++; 
      spilledVRegs.push_back(virtReg);

      // get data for all vReg classes
      auto regClassName = getRegClassName(virtReg);
      vRegClassData[regClassName].push_back(virtReg);

      origVRegInfo[virtReg] = false; 
    } 
    else if (isMappedToPhysReg(virtReg)) {
      allocatedVirtRegs++;
      allocatedVRegs.push_back(virtReg);
      origVRegInfo[virtReg] = true;

      auto regClassName = getRegClassName(virtReg);
      auto regAsmName = getPhysRegName(virtReg);

      vRegClassData[regClassName].push_back(virtReg);
      // add to the number of used virtRegs; 
      regClassMap[regClassName].nmap[regAsmName].push_back(virtReg);
    } 
    else {
      errs() << "No information for this virtual Register found!: " << virtReg << '\n';
    } 

  } 

}
                                    
void RegAllocProfiler::calculateProfilerStats() { 

  numUsedVirtRegs = originalVRegSet.size();
   
  for (auto const& pair : regClassMap) {
    // calculate the unique number of GPRs allocated in this class
    regClassData[pair.first].first = pair.second.nmap.size(); 

    for (auto const& mappings: pair.second.nmap) { 
      //calculate the number of virtregs allocated to each class
      regClassData[pair.first].second += mappings.second.size();
    }
  }
}

// checks with the MF to get all the actual virtRegs used in 
// MachineInstructions, and adds them to the original vReg set
void RegAllocProfiler::getOriginalVRegs() {

  for (unsigned i = 0; i < nominalVirtRegs; ++i) { 
    unsigned Reg = Register::index2VirtReg(i);
    if (MRI->reg_nodbg_empty(Reg))
      continue;
    originalVRegSet.push_back(Reg);

  } 

} 

