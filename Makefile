LLVM_VERSION=10.0.0

all: install-llvm profiler_patch ninja-setup build 

install-llvm: 
	git clone https://github.com/hkhajanchi/LLVM_installer.git $(LLVM_VERSION)
	cd $(LLVM_VERSION); make src BACKENDS="X86" EXTRAS="noextra" TESTS="notest"

profiler_patch: 
	cd profiler; tar cf profiler_patch *; mv profiler_patch ../; 
	cd $(LLVM_VERSION)/src/; tar xf ../../profiler_patch;

ninja-setup:
	cd $(LLVM_VERSION); cmake ./src -G Ninja

build:
	cd $(LLVM_VERSION) && ninja llc

clean: 
	rm -rf 10.0.0/; rm -rf profiler_patch
