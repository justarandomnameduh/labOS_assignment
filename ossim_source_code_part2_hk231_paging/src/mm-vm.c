//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */
#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#define PAGING_PAGE_SIZE 4096
/*
 * Allocate physical frames on the MEMRAM device
 */

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
/* Function that enlists a virtual memory region into the free region list for a process.
 * Takes in a pointer to the process' memory management structure and the new region to add. */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct rg_elmt)
{
  // Get the head of the current free region list
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  // Validate the region parameters
  if (rg_elmt.rg_start >= rg_elmt.rg_end)
    return -1;

  // Add the new region to the start of the list
  if (rg_node != NULL)
    rg_elmt.rg_next = rg_node;
  mm->mmap->vm_freerg_list = &rg_elmt;

  return 0;
}


/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
/* Function that returns the virtual memory area corresponding to a given VMA number.
 * Takes in a pointer to the process' memory management structure and a VMA number. */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  // Get the first VMA in the list
  struct vm_area_struct *pvma= mm->mmap;

  // If there are no VMAs, return NULL
  if(mm->mmap == NULL)
    return NULL;

  int vmait = 0;
  
  // Traverse the list until the desired VMA is reached
  while (vmait < vmaid)
  {
    // If there are not enough VMAs in the list, return NULL
    if(pvma == NULL)
      return NULL;
    
    pvma = pvma->vm_next;
    vmait++;
  }

  return pvma;
}


/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if(rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/* Function that allocates a region of memory for a process.
 * Takes in a pointer to the calling process, a VMA ID to allocate memory for,
 * a memory region ID, the desired size of the allocation, and a pointer
 * to return the starting address of the allocated memory. */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
  // Attempt to allocate memory at the top of the VMA
  struct vm_rg_struct rgnode;

  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    // If successful, update symbol table with allocated memory range
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

    // Return the starting address of the allocated memory
    *alloc_addr = rgnode.rg_start;

    return 0;
  }

  // If there is not enough free space in the VMA, attempt to increase its limit
  // Get virtual memory area corresponding to vmaid
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  
  // Store current value of break pointer in old_sbrk variable
  int old_sbrk = cur_vma->sbrk;
  
  // Round up size to nearest page size and store in inc_sz variable
  int inc_sz = PAGING_PAGE_ALIGNSZ(size);
  
  // Increase virtual memory area limit by inc_sz, returns 0 on success
  int inc_limit_ret = inc_vma_limit(caller, vmaid, inc_sz);
  
  // Check if increasing limit failed, return -1 indicating failure
  if (inc_limit_ret != 0) {
    // Failed to allocate memory
    return -1;
  }
  
  // Increase the break pointer by inc_sz to allocate memory
  caller->bp += inc_sz;
  
  // Update symbol table with allocated memory range
  caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
  caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size;
  
  // Return the starting address of the allocated memory
  *alloc_addr = old_sbrk;
  
  // Return 0 indicating successful allocation
  return 0;
  



/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
    struct vm_rg_struct rgnode;

    if(rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ) {
        return -1;
    }

    /* Get the virtual memory region to be freed */
    struct vm_area_struct *vma = caller->mm->mmap;

    while(vma != NULL && vma->vm_next != NULL) {
        if(vmaid == vma->vm_id) {
            break;
        }
        vma = vma->vm_next;
    }

    /* Check that the requested virtual memory region exists */
    if(vma == NULL || vma->vm_id != vmaid) {
        return -1;
    }



    /* Add the freed memory range to the freerg_list */
    rgnode.start = PAGE_SIZE * (vma->vm_start + rgid);
    rgnode.end = rgnode.start + PAGE_SIZE;
    enlist_vm_freerg_list(caller->mm, rgnode);

    return 0;
}

/*pgalloc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int pgalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  int addr;

  /* By default using vmaid = 0 */
  return __alloc(proc, 0, reg_index, size, &addr);
}

/*pgfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int pgfree_data(struct pcb_t *proc, uint32_t reg_index)
{
   return __free(proc, 0, reg_index);
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
    uint32_t pte = mm->pgd[pgn];

    if (!PAGING_PAGE_PRESENT(pte)) {
        /* Page is not present, replace it with a new page */
        int vicpgn, swpfpn;

        /* Find victim page to be replaced */
        find_victim_page(caller->mm, &vicpgn);

        /* Get a free frame in MEMSWP */
        MEMPHY_get_freefp(caller->active_mswp, &swpfpn);

        /* Swap victim page out to MEMSWP and swap new page in from MEMSWP */
        __swap_cp_page(mm, vicpgn, swpfpn);
        __swap_cp_page(mm, pgn, PAGING_SWP(pte));

        /* Update page table entries */
        pte_set_swap(&mm->pgd[pgn], 0);
        pte_set_fpn(&mm->pgd[pgn], PAGING_SWP(swpfpn));

        /* Add page to process's FIFO queue */
        enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
    }

    *fpn = PAGING_FPN(pte);

    return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess 
 *@value: value
 *
 */
/* Function that reads the value at a given virtual address in a memory region.
 * Takes in a pointer to the memory region, the address to access,
 * a pointer for the returned data, and the calling process. */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  // Get the page from MEMRAM or swap from MEMSWAP if needed
  if(pg_getpage(mm, pgn, &fpn, caller) != 0) 
    return -1; // Invalid page access

  // Calculate physical address of accessed data
  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  // Read the data at the calculated physical address from MEMPHY
  MEMPHY_read(caller->mram, phyaddr, data);

  return 0;
}


/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess 
 *@value: value
 *
 */
/* Function that sets a given value at a specified virtual address in a memory region.
 * Takes in a pointer to the memory region, the address to set the value,
 * the new value, and the calling process. */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  // Get the page from MEMRAM or swap from MEMSWAP if needed
  if(pg_getpage(mm, pgn, &fpn, caller) != 0) 
    return -1; // Invalid page access

  // Calculate physical address of location to set value
  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  // Set the value at the calculated physical address in MEMPHY
  MEMPHY_write(caller->mram, phyaddr, value);

  return 0;
}


/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region 
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *
 */
/* Function that reads data from a specified offset in a virtual memory region.
 * Takes in the calling process, the virtual memory area ID, the memory region ID,
 * the offset to read from, and a pointer to store the read data. */
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
{
  // Get the symbol region and VMA with the given IDs
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  // Check for invalid memory identification
  if(currg == NULL || cur_vma == NULL)
      return -1;

  // Get the value at the specified offset in the memory region using pg_getval()
  pg_getval(caller->mm, currg->rg_start + offset, data, caller);

  return 0;
}



/*pgwrite - PAGING-based read a region memory */
/* Function that reads a byte of data from the specified offset in a virtual memory region,
 * and stores it in a destination register of the calling process.
 * Takes in the calling process, the index of the source register containing the virtual memory region ID,
 * the offset to read from, and the index of the destination register to store the read data. */
int pgread(struct pcb_t *proc, uint32_t source, uint32_t offset, uint32_t destination) 
{
  BYTE data;

  // Call __read() with the given parameters to read from the virtual memory region
  int val = __read(proc, 0, source, offset, &data);

  // Store the read data in the destination register
  destination = (uint32_t) data;

#ifdef IODUMP
  // Print the read data if IODUMP flag is set
  printf("read region=%d offset=%d value=%d\n", source, offset, data);

#ifdef PAGETBL_DUMP
  // Print max page table if PAGETBL_DUMP flag is also set
  print_pgtbl(proc, 0, -1);
#endif
  // Dump physical memory if MEMPHY_DUMP flag is set
  MEMPHY_dump(proc->mram);
#endif

  return val;
}


/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region 
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *
 */
/* Function that writes a byte of data to the specified offset in a virtual memory region.
 * Takes in the calling process, the ID of the virtual memory area containing the region to write to,
 * the ID of the register containing the virtual memory region start address,
 * the offset to write to, and the data value to write. */
int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
{
  // Get the symbol for the region to write to from the calling process's memory map
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);

  // Get the virtual memory area corresponding to the given ID from the calling process's memory map
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  // Return -1 if either the region or the virtual memory area is invalid
  if(currg == NULL || cur_vma == NULL) /* Invalid memory identify */
      return -1;

  // Call pg_setval() with the given parameters to set the data value in the virtual memory region
  pg_setval(caller->mm, currg->rg_start + offset, value, caller);

  return 0;
}


/*pgwrite - PAGING-based write a region memory */
/* Function that writes a byte of data to a virtual memory region at the specified offset.
 * Takes in the executing process, the data value to write, the index of the register containing the virtual memory region start address,
 * and the offset within the region to write to. */
int pgwrite(
        struct pcb_t * proc,
        BYTE data,
        uint32_t destination,
        uint32_t offset)
{
#ifdef IODUMP
  // Print information about the write operation if IODUMP is defined
  printf("write region=%d offset=%d value=%d\n", destination, offset, data);

  // Print the page table for the executing process if PAGETBL_DUMP is defined
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif

  // Dump the contents of the physical memory if IODUMP is defined
  MEMPHY_dump(proc->mram);
#endif

  // Call __write() with the given parameters to write the data to the virtual memory region
  return __write(proc, 0, destination, offset, data);
}



/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  int pagenum, fpn;
  uint32_t pte;


  for(pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte= caller->mm->pgd[pagenum];

    if (!PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    } else {
      fpn = PAGING_SWP(pte);
      MEMPHY_put_freefp(caller->active_mswp, fpn);    
    }
  }

  return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
/* Function that returns a new virtual memory area node for the calling process from brk() system call.
 * Takes in a pointer to the calling process, the virtual memory area ID, the size of the memory requested, 
 * and the aligned size of the memory requested. */
struct vm_rg_struct* get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz)
{
  struct vm_rg_struct * newrg;

  // Get the virtual memory area for the given ID number
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  // Allocate memory for the new virtual memory area node
  newrg = malloc(sizeof(struct vm_rg_struct));

  // Set the start and end addresses for the new node based on the current sbrk value and requested size
  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end = newrg->rg_start + size;

  return newrg; // Return the newly created virtual memory area node
}


/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
/* Function that validates if the memory area with the given start and end addresses overlaps 
 * with any of the current virtual memory areas of the calling process.
 * Returns 1 if overlap is detected, otherwise returns 0. */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend)
{
  // Get the first virtual memory area of the calling process
  struct vm_area_struct *vma = caller->mm->mmap;

  while (vma != NULL) {
    // Check if the given memory area overlaps with the current virtual memory area
    if ((vmastart < vma->end_addr && vmaend > vma->start_addr) || 
        (vmaend > vma->start_addr && vmastart < vma->end_addr)) {
      // Overlap found, return 1
      return 1;
    }

    // Move to the next virtual memory area in the list
    vma = vma->vm_next;
  }

  // No overlap found, return 0
  return 0;
}



/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size 
 *
 */
/* Function that increases the limit of the virtual memory area with the given id, by a specified amount.
 * Returns 0 if successful, -1 if there is overlap with another virtual memory area or if allocation fails. */
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz)
{
  // Allocate memory for a new virtual memory region
  struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));

  // Round up the increase size to the nearest page size multiple
  int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);

  // Calculate the number of pages required
  int incnumpage =  inc_amt / PAGING_PAGESZ;

  // Get the virtual memory area node at the current break of the calling process
  struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);

  // Get the virtual memory area with the given ID
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  // Keep track of the old end address of the virtual memory area
  int old_end = cur_vma->vm_end;

  /* Validate if there is overlap between the new virtual memory area and any existing virtual memory area.
   * If there is overlap, return -1 indicating failure. */
  if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0)
    return -1;

  /* If there is no overlap, extend the virtual memory area by the specified amount.
   * Map the memory to physical RAM using the obtained virtual memory region.
   * If allocation fails, return -1 indicating failure. */
  cur_vma->vm_end += inc_sz;
  
  if (vm_map_ram(caller, area->rg_start, area->rg_end, old_end, incnumpage , newrg) < 0)
    return -1;

  // Return success
  return 0;
}


/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
/* Function that finds the oldest page in the FIFO list of a process, 
 * sets the pointer to it, and frees the memory. Returns 0 if successful. */
int find_victim_page(struct mm_struct *mm, int *retpgn) 
{
  // Get a pointer to the first (oldest) page in the FIFO list
  struct pgn_t *pg = mm->fifo_pgn;

  // Set the return value to the ID of the oldest page
  *retpgn = pg->pgn;

  // Free the memory allocated for the oldest page
  free(pg);

  // Return success
  return 0;
}



/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size 
 *
 */
/* Function that searches the free vm region list of a process to find a region large enough to hold a given size, sets the start and end addresses in the provided newrg struct, and updates the free region list. Returns 0 if successful, -1 otherwise. */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  // Get the current vm area based on its ID
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  // Get a pointer to the first free region in the list
  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

  if (rgit == NULL)
    return -1;   // Return failure if there are no free regions

  /* Initialize the new region with invalid addresses */
  newrg->rg_start = newrg->rg_end = -1;

  /* Traverse the list of free regions to find a fit space */
  while (rgit != NULL)
  {
    if (rgit->rg_start + size <= rgit->rg_end)
    { /* Current region has enough space */
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;

      /* Update the left space in the chosen region */
      if (rgit->rg_start + size < rgit->rg_end)
      { /* There is some space left in the region */
        rgit->rg_start = rgit->rg_start + size;
      }
      else
      { /* Use up all space, remove current node */
        /* Clone the next region node */
        struct vm_rg_struct *nextrg = rgit->rg_next;

        /* Clone values */
        if (nextrg != NULL)
        {
          rgit->rg_start = nextrg->rg_start;
          rgit->rg_end = nextrg->rg_end;

          rgit->rg_next = nextrg->rg_next;

          free(nextrg);   // Free the cloned region
        }
        else
        { /* End of free list */
          rgit->rg_start = rgit->rg_end;   // Create a dummy, size 0 region
          rgit->rg_next = NULL;
        }
      }
    }
    else
    {
      rgit = rgit->rg_next;   // Traverse next region
    }
  }

 if(newrg->rg_start == -1) // new region not found
   return -1;

 return 0;   // Return success
}


//#endif
