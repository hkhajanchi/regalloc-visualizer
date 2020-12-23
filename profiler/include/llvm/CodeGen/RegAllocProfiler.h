#include "llvm/ADT/IndexedMap.h"
#include "llvm/CodeGen/Register.h"
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
#include "set"

using namespace llvm;

// RegAllocProfiler - parses VRM & MRI data to collect Register Allocation performance statistics
// i.e number of virtual registers allocated to physical registers, stack slots, etc
// TODO: Add more detailed info about performance stats
class RegAllocProfiler {

  private: 
    // the nominal number of virtRegs - the actual number of virtRegs used in MachineInstrs is less
    unsigned nominalVirtRegs;

    // the number of actually used virtRegs counted by Machine instruction use
    unsigned numUsedVirtRegs; 

    // allocated number of virtRegs
    unsigned allocatedVirtRegs;

    unsigned numSpilledVirtRegs;

    // Pointer to current machine function to recover machine instructions
    MachineFunction* MF;

    // To get target register names and subclasses
    const TargetRegisterInfo* TRI;

    // To get post reg-alloc virtReg mappings
    VirtRegMap* VRM; 

    // Useful for getting more in-depth virtReg info
    MachineRegisterInfo* MRI; 

    // VRegInfo - copy of the VRegInfo stored in MRI, useful to keep a local copy
    // Holds information about VReg register class and use/defs in machine instructions
    const IndexedMap<std::pair<RegClassOrRegBank, MachineOperand*>,
               VirtReg2IndexFunctor>& 
              VRegInfo; 
    
    // Virt2PhysMap - reference to the DS held in VirtRegMap 
    // Contains info about virtReg->physReg mappings
    const IndexedMap<Register, VirtReg2IndexFunctor>& regMap; 

    // stackSlotMap - Map holding spill data from VRM 
    const IndexedMap<int, VirtReg2IndexFunctor> stackSlotMap; 


    // Containers to hold references for all allocated/spilled vars
    std::vector<Register> allocatedVRegs; 
    std::vector<Register> spilledVRegs;

    // get all vregClassData
    std::map<std::string, std::vector<Register>> vRegClassData;

    // registerNameMap - maps all the individual phys reg names to the virtual registers assigned to them
    // map[register_name] ---> {all virtRegs allocated to this register}
    struct regNameMap {
      std::map<StringRef, std::vector<unsigned>> nmap; 
    };

    // RegClassMap - a nested map that holds mapping information for each register class
    // Basically lists all virtual registers mapped to a specific register class 
    // map[class_name] ---> (regNameMap) 
    std::map<std::string, regNameMap> regClassMap; 

    // Another quick map holding data relating a reg class to the number of unique registers allocated within it
    // and the total number of virtRegs allocated to ti
    // map[class_name] ---> (numUniqueRegsAllocated, totalVirtRegsAllocated)
    std::map<StringRef, std::pair<unsigned, unsigned>> regClassData;


    // Vector containing the set of all the originally enqueued virtRegs
    std::vector<Register> originalVRegSet;

    // origVRegInfo -> contains allocation status for the all non-dbg virtual registers that were originally enqueued at the 
    // start of regalloc. Designed to exclude virtRegs created by splitting & spilling.
    // Each vReg is mapped to a binary encoding ->  1 for physReg allocation, 0  for stack slot assignment
    std::map<Register, bool> origVRegInfo;


    // 11/10 - added DS for all split-marked vRegs 
    std::set<Register>* splitMarkedVRegs;
  
    // Determines the original virtual register set in the MachineFunction
    // before splitting/spilling
    void getOriginalVRegs(); 

    // populates the regClassMap by parsing each virtual register and 
    // adding it to the mapped name register
    void populateRegisterClassMap();

    // Calculates Profiler stats after RegClassMap is populated
    void calculateProfilerStats();

  public: 
    // Constructor - requires pointers in order to work
    RegAllocProfiler (MachineFunction* MF, 
                      const TargetRegisterInfo* TRI, 
                      VirtRegMap* VRM, 
                      MachineRegisterInfo* MRI,
                      std::set<Register>* splitMarkedVRegs);
                       
    // Quick Accessor method for originalVRegs
    std::vector<Register> originalVRegs() { return originalVRegSet; }

    // IMPORTANT: this needs to be called BEFORE allocatePhysRegs() in the RA 
    // MRI dynamically increases because of spilled virtRegs - call this function before
    // allocatePhysRegs() in order to accurately get the original vreg set
    void init();
     
    // Has to be called after allocatePhysRegs() in order to accurately compute
    // RA statistics
    void computeStats();
     
    // Returns whether the virtual register is actually used 
    // in the Machine Instructions of the function
    bool isUsedInFunction (Register reg);

    // Checks whether the virtReg has a physical register mapping
    bool isMappedToPhysReg (Register reg); 

    // Checks whether a virtReg is spilled to the stack
    bool isSpilled (Register reg);

    // returns the numberspace physReg mapping of the virtReg
    unsigned getPhysRegMapping(Register reg);

    // Gets the name of the register class of the virtReg
    std::string getRegClassName(Register reg); 

    // Gets the name of the physical register assigned to the virtReg
    StringRef getPhysRegName (Register reg); 
  
    // 11/10 - gets the allocation status for a originally enqueued vReg
    // 0 if split/spilled, 1 if allocated to a physReg
    bool getRegAllocation (Register reg);

    // Dumps all mappings for originalVRegSet
    void dumpOrigVRegMappings();

    // Dump all the machine instructions associated with a virtReg
    void dumpVirtRegInstructions (Register reg); 

    // Dump all the Profiler stats - #allocated virtRegs, #unique GPRs allocated/class
    void dumpProfilerStats(); 

    // writes debug stats to file - I'll implement file I/O later
    void writeDebugStats(MachineFunction& MF); 
    
    // Dump profiler stats to file
    void dumpProfStatsToFile(std::string fname);
    
    // dumps all regalloc statistics, and everything in the registerNameMap
    void dump();
};

