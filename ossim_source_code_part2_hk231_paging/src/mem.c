#include "mem.h"
#include "stdlib.h"
#include "string.h"
#include <pthread.h>
#include <stdio.h>

static BYTE _ram[RAM_SIZE];

static struct {
	uint32_t proc;	// ID of process currently uses this page
	int index;	// Index of the page in the list of pages allocated
			// to the process.
	int next;	// The next page in the list. -1 if it is the last
			// page.
} _mem_stat [NUM_PAGES];	// List of page info

static pthread_mutex_t mem_lock;

void init_mem(void) {
	memset(_mem_stat, 0, sizeof(*_mem_stat) * NUM_PAGES);
	memset(_ram, 0, sizeof(BYTE) * RAM_SIZE);
	pthread_mutex_init(&mem_lock, NULL);
}

/* get offset of the virtual address */
static addr_t get_offset(addr_t addr) {
	return addr & ~((~0U) << OFFSET_LEN);
}

/* get the first layer index */
static addr_t get_first_lv(addr_t addr) {
	return addr >> (OFFSET_LEN + PAGE_LEN);
}

/* get the second layer index */
static addr_t get_second_lv(addr_t addr) {
	return (addr >> OFFSET_LEN) - (get_first_lv(addr) << PAGE_LEN);
}

/* Search for page table table from the a segment table */
static struct trans_table_t * get_trans_table(
		addr_t index, 	// Segment level index
		struct page_table_t * page_table) { // first level table

	/* DO NOTHING HERE. This mem is obsoleted */

	int i;
	for (i = 0; i < page_table->size; i++) {
		// Enter your code here
		if(page_table->table[i].v_index == index)
			return page_table->table[i].next_lv;
	}
	return NULL;

}

/* Translate virtual address to physical address. If [virtual_addr] is valid,
 * return 1 and write its physical counterpart to [physical_addr].
 * Otherwise, return 0 */
static int translate(
		addr_t virtual_addr, 	// Given virtual address
		addr_t * physical_addr, // Physical address to be returned
		struct pcb_t * proc) {  // Process uses given virtual address

	/* Offset of the virtual address */
	addr_t offset = get_offset(virtual_addr);
        offset++; offset--;
	/* The first layer index */
	addr_t first_lv = get_first_lv(virtual_addr);
	/* The second layer index */
	addr_t second_lv = get_second_lv(virtual_addr);

	/* Search in the first level */
	struct trans_table_t * trans_table = NULL;
	trans_table = get_trans_table(first_lv, proc->page_table);
	if (trans_table == NULL) {
		return 0;
	}

	int i;
	for (i = 0; i < trans_table->size; i++) {
		if (trans_table->table[i].v_index == second_lv) {
			/* DO NOTHING HERE. This mem is obsoleted */
			/* Edited */
			physical_addr = trans_table->table[i].p_index * PAGE_SIZE + offset;
			return 1;
		}
	}
	return 0;
}

/* This function allocate [size] byte of memory to the given process [proc]
 * and return the address of the first byte in the allocated memory region.
 * Params:
 * 		@size: size of memory to be allocated (in bytes)
 * 		@proc: pointer to the process for which memory is being allocated
 * Returns:
 * 		@ret_mem: address of the first byte in the allocated memory region,
 * 		if memory is insufficient, return 0 instead. 
 * */
addr_t alloc_mem(uint32_t size, struct pcb_t * proc) {
	pthread_mutex_lock(&mem_lock);
	addr_t ret_mem = 0;
	/* DO NOTHING HERE. This mem is obsoleted */

	uint32_t num_pages = (size % PAGE_SIZE) ? size / PAGE_SIZE :
		size / PAGE_SIZE + 1; // Number of pages we will use
	int mem_avail = 0; // We could allocate new memory region or not?

	/* First we must check if the amount of free memory in
	 * virtual address space and physical address space is
	 * large enough to represent the amount of required
	 * memory. If so, set 1 to [mem_avail].
	 * Hint: check [proc] bit in each page of _mem_stat
	 * to know whether this page has been used by a process.
	 * For virtual memory space, check bp (break pointer).
	 * */
	// TODO
	int i, j;
	int num_avail_pages = 0;
	for (i = 0; i < NUM_PAGES; i++) {
		if(_mem_stat[i].proc == 0) {
			num_avail_pages++;
			if (num_avail_pages == num_pages &&	// collect enough required free page
				proc->bp + num_pages * PAGE_SIZE <= RAM_SIZE) {
				// the boundary must be smaller than RAM_SIZE
					mem_avail = 1;
					break;
				}
		}
	}

	if (mem_avail) {
		/* We could allocate new memory region to the process */
		ret_mem = proc->bp;
		proc->bp += num_pages * PAGE_SIZE; // address of the top of the heap
		/* Update status of physical pages which will be allocated
		 * to [proc] in _mem_stat. Tasks to do:
		 * 	- Update [proc], [index], and [next] field
		 * 	- Add entries to segment table page tables of [proc]
		 * 	  to ensure accesses to allocated memory slot is
		 * 	  valid. */
		// TODO
		int num_alloc_pages = 0;	
		int prev_page_id;
		addr_t cur_v_addr;
		int trans_id, page_id;
		struct page_table_t * proc_page_table;	// a table of trans_table
		struct trans_table_t * trans_table; // a table of phys-virt address pairs
		for(i = 0; i < NUM_PAGES; i++) {
			// Iterate through list of pages to assign free page
			// to current process
			if(_mem_stat[i].proc == 0) { // haven't assign by any proc
				_mem_stat[i].proc = proc->pid;
				_mem_stat[i].index = num_alloc_pages;
				if(_mem_stat[i].index != 0) // Not the first page, so linked from previous page
					_mem_stat[prev_page_id].next = i;
				prev_page_id = i;
				int found = 0;
				// Search for paging entry if already exist in the page table
				proc_page_table = proc->page_table;
				if(proc_page_table->table[0].next_lv == NULL) // page table with no trans_table
					proc_page_table->size = 0;
				cur_v_addr = ret_mem + (num_alloc_pages << OFFSET_LEN);
				page_id = get_first_lv(cur_v_addr);
				trans_id = get_second_lv(cur_v_addr);
				// Find the trans_table that keep the page of current process memory
				for(j = 0; j < proc_page_table->size; j++)
					if (proc_page_table->table[j].v_index == page_id &&
						proc_page_table->table[i].next_lv->size < (1 << SECOND_LV_LEN)) {
						// Found the page and there are still available space
						// add new row of phys-virt address pair
						found = 1;
						trans_table = proc_page_table->table[j].next_lv;
						trans_table->table[trans_table->size].v_index = trans_id;
						trans_table->table[trans_table->size].p_index = i;
						trans_table->size++;
						break;
					}
				if(!found) {
				// No available page_table
					proc_page_table->table[proc_page_table->size].v_index = page_id;
					proc_page_table->table[proc_page_table->size].next_lv = 
						(struct trans_table_t*)malloc(sizeof(struct trans_table_t));
						// Allocate memory for a new page/trans_table
					proc_page_table->table[proc_page_table->size].next_lv->table[0].v_index = trans_id;
					proc_page_table->table[proc_page_table->size].next_lv->table[0].p_index = i;
					proc_page_table->table[proc_page_table->size].next_lv->size = 1;
					proc_page_table->size++;
				}
				num_alloc_pages++;
				if(num_alloc_pages == num_pages) {
				// Reach the final page
					_mem_stat[i].next = -1;
					break;
				}
			}
		}
	}
	else {
		/* In case of no suitable space, we need to lift up the
		 * barrier sbrk, which may need some physical frames and
		 * then mapped with Page Table Entry. */
		// There is no sufficient memory, due to the requirement,
		// 	no change in the memory is added
	}
	pthread_mutex_unlock(&mem_lock);
	return ret_mem;
}

int free_mem(addr_t address, struct pcb_t * proc) {
	/* DO NOTHING HERE. This mem is obsoleted */
	/* Keep free list for further alloc request */
	// TODO maybe...
	return 0;
}

int read_mem(addr_t address, struct pcb_t * proc, BYTE * data) {
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc)) {
		*data = _ram[physical_addr];
		return 0;
	}else{
		return 1;
	}
}

int write_mem(addr_t address, struct pcb_t * proc, BYTE data) {
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc)) {
		_ram[physical_addr] = data;
		return 0;
	}else{
		return 1;
	}
}

void dump(void) {
	int i;
	for (i = 0; i < NUM_PAGES; i++) {
		if (_mem_stat[i].proc != 0) {
			printf("%03d: ", i);
			printf("%05x-%05x - PID: %02d (idx %03d, nxt: %03d)\n",
				i << OFFSET_LEN,
				((i + 1) << OFFSET_LEN) - 1,
				_mem_stat[i].proc,
				_mem_stat[i].index,
				_mem_stat[i].next
			);
			int j;
			for (	j = i << OFFSET_LEN;
				j < ((i+1) << OFFSET_LEN) - 1;
				j++) {

				if (_ram[j] != 0) {
					printf("\t%05x: %02x\n", j, _ram[j]);
				}

			}
		}
	}
}
