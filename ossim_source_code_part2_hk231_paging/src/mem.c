
#include "mem.h"
#include "stdlib.h"
#include "string.h"
#include <pthread.h>
#include <stdio.h>

static BYTE _ram[RAM_SIZE];

static struct
{
	uint32_t proc; // ID of process currently uses this page
	int index;	   // Index of the page in the list of next_lv allocated
				   // to the process.
	int next;	   // The next page in the list. -1 if it is the last
				   // page.
} _mem_stat[NUM_PAGES];

static pthread_mutex_t mem_lock;

// This function initializes the memory by setting all pages in _mem_stat to 0 and clearing the RAM by setting all bytes to 0.
void init_mem(void)
{
	memset(_mem_stat, 0, sizeof(*_mem_stat) * NUM_PAGES); // Clears all pages of physical memory by filling it with 0's.
	memset(_ram, 0, sizeof(BYTE) * RAM_SIZE);			  // Clears all bytes of the RAM by filling it with 0's.
	pthread_mutex_init(&mem_lock, NULL);				  // Initializes the mutex used to synchronize access to memory.
}

/* get offset of the virtual address */
static addr_t get_offset(addr_t addr)
{
	return addr & ~((~0U) << OFFSET_LEN);
}

/* get the first layer index */
static addr_t get_first_lv(addr_t addr)
{
	return addr >> (OFFSET_LEN + PAGE_LEN);
}

/* get the second layer index */
static addr_t get_second_lv(addr_t addr)
{
	return (addr >> OFFSET_LEN) - (get_first_lv(addr) << PAGE_LEN);
}

/* Search for page table table from the a segment table */
/*
 * This function returns the translation table for a given segment level [index].
 * It searches through each row of the segment table [page_table] and returns
 * the next-level page table whose virtual index matches the provided index.
 *
 * Params:
 *     @index: Segment level index
 *     @page_table: First level page table
 *
 * Returns:
 *     Pointer to the next level page table whose virtual index matches the provided index,
 *     otherwise returns NULL if no match is found.
 */
static struct trans_table_t *get_trans_table(addr_t index, struct page_table_t *page_table)
{
	int i;
	for (i = 0; i < page_table->size; i++)
	{ // Iterate every row in the provided page table
		if (index == page_table->table[i].v_index)
		{										 // Check if the virtual index of the current row matches the provided segment index
			return page_table->table[i].next_lv; // Return the next level page table for that matching row
		}
	}
	return NULL; // Return NULL if no matching row is found
}

/*
 * The following function translates a virtual address to its corresponding physical address using
 * the two level page table and provided process's PCB.
 * If translation is successful, the function returns 1 and stores the physical address at [physical_addr].
 * Otherwise, it returns 0.
 */
static int translate(
	addr_t virtual_addr,   // Given virtual address
	addr_t *physical_addr, // Physical address to be returned
	struct pcb_t *proc)
{ // Process that uses the given virtual address

	// Extract offset, first level index and second level index from the given virtual address
	addr_t offset = get_offset(virtual_addr);
	addr_t first_lv = get_first_lv(virtual_addr);
	addr_t second_lv = get_second_lv(virtual_addr);

	// Get the first level page table from the given process's PCB
	struct trans_table_t *trans_table = NULL;
	trans_table = get_trans_table(first_lv, proc->page_table);

	// Check if the first level page table exists and has the second level page table entry for the given address
	if (trans_table == NULL)
	{
		return 0;
	}
	int i;
	for (i = 0; i < 1 << PAGE_LEN; i++)
	{
		if (trans_table->table[i].v_index == second_lv)
		{
			/* TODO: Concatenate the offset of the virtual addess
			 * to [p_index] field of trans_table->table[i] to
			 * produce the correct physical address and save it to
			 * [*physical_addr]  */

			// Concatenate the p_index value with the offset and store it in *physical_addr
			*physical_addr = trans_table->table[i].p_index * PAGE_SIZE + offset;
			return 1;
		}
	}
	// If translation fails, return 0
	return 0;
}

/*
 * This function allocates [size] bytes of memory to the given process [proc].
 * It returns the address of the first byte in the allocated memory region.
 *
 * Params:
 *     @size: Size of memory to be allocated (in bytes)
 *     @proc: Pointer to the process for which memory is being allocated
 *
 * Returns:
 *     Address of the first byte in the allocated memory region,
 *     otherwise returns 0 if insufficient memory is available
 */
addr_t alloc_mem(uint32_t size, struct pcb_t *proc)
{
	pthread_mutex_lock(&mem_lock); // Acquire lock on the memory

	addr_t ret_mem = 0; // Address of the first byte in the allocated memory region

	uint32_t num_next_lv = ((size % PAGE_SIZE) == 0) ? size / PAGE_SIZE : size / PAGE_SIZE + 1; // Number of next level page tables we will use
	int mem_avail = 0;																			// Determines whether we can allocate new memory or not

	/*
	 * We will first check if there is sufficient free memory available.
	 * For virtual memory space, we check if there is enough space between break pointer and RAM_SIZE.
	 * For physical memory space, we check if there are pages with their proc bit set to 0.
	 */
	int i;
	int num_avail_next_lv = 0;
	for (i = 0; i < NUM_PAGES; i++)
	{ // Iterate through every page in _mem_stat array
		if (_mem_stat[i].proc == 0)
		{						 // If the current page is free
			num_avail_next_lv++; // Increase count of free pages
			if (num_avail_next_lv == num_next_lv && proc->bp + num_next_lv * PAGE_SIZE <= RAM_SIZE)
			{				   // If we have enough free pages and enough space in virtual memory
				mem_avail = 1; // We can allocate new memory
				break;
			}
		}
	}

	if (mem_avail)
	{										 // If we can allocate new memory
		ret_mem = proc->bp;					 // Set starting address of allocated memory region as break pointer of process
		proc->bp += num_next_lv * PAGE_SIZE; // Update break pointer of the process to account for newly allocated memory

		/*
		 * Next, we will update the physical memory state and add entries to process's segment table page tables,
		 * so that accesses to the allocated memory slot is valid.
		 */
		int num_alloc_next_lv = 0; // Number of next-level page tables we have traversed through while allocating memory
		int pre_index;			   // Index of the previous page in the list
		addr_t cur_vir_addr;	   // Virtual memory address of the current page
		int page_idx, trans_idx;   // pagination indices
		for (i = 0; i < NUM_PAGES; i++)
		{ // Iterate through every page in _mem_stat array
			if (_mem_stat[i].proc == 0)
			{											// If the current page is free
				_mem_stat[i].proc = proc->pid;			// Mark page as used by given process
				_mem_stat[i].index = num_alloc_next_lv; // Set index for this page

				if (_mem_stat[i].index != 0)	   // If not the first page being allocated
					_mem_stat[pre_index].next = i; // Update "next" field of previous page struct to point to current page

				pre_index = i; // Store current page index as previous index for next iteration

				int found = 0;										// Determines whether a matching entry is already present in the page table
				struct page_table_t *page_table = proc->page_table; // Pointer to the process's page table
				if (page_table->table[0].next_lv == NULL)			// If this is the first page table being added to the process's segment table
					page_table->size = 0;

				cur_vir_addr = ret_mem + (num_alloc_next_lv << OFFSET_LEN); // Virtual memory address of the current page

				page_idx = get_first_lv(cur_vir_addr);	 // Get page index for current virtual address
				trans_idx = get_second_lv(cur_vir_addr); // Get translation index for current virtual address

				int j;
				for (j = 0; j < page_table->size; j++)
				{ // Iterate through every row in the segment table
					if (page_table->table[j].v_index == page_idx)
					{																		  // If a matching entry is found in the page table
						struct trans_table_t *cur_trans_table = page_table->table[j].next_lv; // Pointer to next-level page table

						cur_trans_table->table[cur_trans_table->size].v_index = trans_idx; // Set virtual index for new entry
						cur_trans_table->table[cur_trans_table->size].p_index = i;		   // Set physical index for new entry

						cur_trans_table->size++; // Increase size of next-level page table

						found = 1; // Mark flag as true to indicate that we have found a matching entry
						break;
					}
				}

				if (!found)
				{																												// If no matching entry is found in the page table
					page_table->table[page_table->size].v_index = page_idx;														// Set virtual index for new entry
					page_table->table[page_table->size].next_lv = (struct trans_table_t *)malloc(sizeof(struct trans_table_t)); // Allocate memory for next-level page table

					page_table->table[page_table->size].next_lv->table[0].v_index = trans_idx; // Set virtual index for new entry in next-level page table
					page_table->table[page_table->size].next_lv->table[0].p_index = i;		   // Set physical index for new entry in next-level page table

					page_table->table[page_table->size].next_lv->size = 1; // Set size of next-level page table to 1

					page_table->size++; // Increase size of segment table
				}

				num_alloc_next_lv++; // Increment number of next-level page tables we have traversed through while allocating memory
				if (num_alloc_next_lv == num_next_lv)
				{							// If we have allocated enough memory pages
					_mem_stat[i].next = -1; // Mark "next" field of last page as -1
					break;					// Break out of loop
				}
			}
		}
	}

	pthread_mutex_unlock(&mem_lock); // Release lock on the memory
	return ret_mem;					 // Return starting address of allocated memory region, or 0 if insufficient memory is available.
}

int free_mem(addr_t address, struct pcb_t *proc)
{
	/* DO NOTHING HERE. This mem is obsoleted */
	return 0;
}

/*
 * The following function reads the data stored in memory at the given address
 * by translating the virtual address to physical address using the given process's PCB.
 * If translation is successful, it returns 0 and the data is stored in *data.
 * Otherwise, it returns 1.
 */
int read_mem(addr_t address, struct pcb_t *proc, BYTE *data)
{
	addr_t physical_addr;
	// Translate virtual address to physical address
	if (translate(address, &physical_addr, proc))
	{
		// If translation succeeds, get and store the data at physical address in *data
		*data = _ram[physical_addr];
		return 0;
	}
	else
	{
		// If translation fails, return error code (1)
		return 1;
	}
}

/*
 * The following function writes the given data to the memory location specified by the given address.
 * Translation of the virtual address to physical address is done using the given process's PCB.
 * If translation is successful, data is written to RAM and the function returns 0.
 * If translation fails, it returns 1.
 */
int write_mem(addr_t address, struct pcb_t *proc, BYTE data)
{
	addr_t physical_addr;
	// Translate virtual address to physical address
	if (translate(address, &physical_addr, proc))
	{
		// If translation succeeds, store the given data at the physical address in RAM
		_ram[physical_addr] = data;
		return 0;
	}
	else
	{
		// If translation fails, return error code (1)
		return 1;
	}
}

/*
 * The following function dumps the memory contents of all allocated pages in human-readable form.
 * It iterates over all memory pages and checks if a page is allocated.
 * If it is allocated, the page number, its start-end addresses, associated PID and corresponding frames are printed.
 * Then, it iterates over all physical addresses in the page and prints their data if non-zero.
 */
void dump(void)
{
	int i;
	// Iterate over all pages
	for (i = 0; i < NUM_PAGES; i++)
	{
		// Check if page is allocated
		if (_mem_stat[i].proc != 0)
		{
			// Print page number, start-end addresses, associated PID and corresponding frames
			printf("%03d: ", i);
			printf("%05x-%05x - PID: %02d (idx %03d, nxt: %03d)\n",
				   i << OFFSET_LEN,
				   ((i + 1) << OFFSET_LEN) - 1,
				   _mem_stat[i].proc,
				   _mem_stat[i].index,
				   _mem_stat[i].next);
			int j;
			// Iterate over all physical addresses in the page
			for (j = i << OFFSET_LEN; j < ((i + 1) << OFFSET_LEN) - 1; j++)
			{
				// If data at the current address is not zero, print its value.
				if (_ram[j] != 0)
				{
					printf("\t%05x: %02x\n", j, _ram[j]);
				}
			}
		}
	}
}
