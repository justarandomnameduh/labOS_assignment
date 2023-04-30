#include "mem.h"
#include "cpu.h"
#include "loader.h"
#include <stdio.h>

/* Main function that loads two processes and runs them, alternating between each instruction.
 * Finally, it dumps the memory state */
int main() {
	// Load program p0 into a PCB named ld
	struct pcb_t * ld = load("input/p0");
	
	// Load program p0 into a separate PCB named proc
	struct pcb_t * proc = load("input/p0");
	
	// Loop over each instruction in proc and ld, running alternate instructions
	unsigned int i;
	for (i = 0; i < proc->code->size; i++) {
		run(proc);
		run(ld);
	}
	
	// Output the final memory state after executing both processes
	dump();
	return 0;
}


