LLVM_VERSION=10.0.0

all: install-llvm ninja profiler_patch install 

install-llvm: 
	git clone https://github.com/scampanoni/LLVM_installer.git $(LLVM_VERSION)
	cd $(LLVM_VERSION); make src BACKENDS="X86" EXTRAS="noextra" TESTS="notest"

ninja:
	cd $(LLVM_VERSION); cmake ./src -G Ninja

profiler_patch: 
	cd profiler; tar cf profiler_patch *; mv profiler_patch ../; 

install: 
	cd $(LLVM_VERSION)/src/; tar xf ../../profiler_patch;
	cd $(LLVM_VERSION) && ninja llc
