// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

/*
 * init_pte - Initialize PTE entry
 */
int init_pte(uint32_t *pte,
             int pre,    // present
             int fpn,    // FPN
             int drt,    // dirty
             int swp,    // swap
             int swptyp, // swap type
             int swpoff) // swap offset
{
  if (pre != 0)
  {
    if (swp == 0)
    { // Non swap ~ page online
      if (fpn == 0)
        return -1; // Invalid setting

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
    }
    else
    { // page swapped
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    }
  }

  return 0;
}

/*
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
int pte_set_swap(uint32_t *pte, int swptyp, int swpoff)
{
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
  SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

  return 0;
}

/*
 * pte_set_swap - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 */
int pte_set_fpn(uint32_t *pte, int fpn)
{
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

  return 0;
}

/*
 * vmap_page_range - map a range of page at aligned address
 */
/* Function that maps a range of physical memory pages to a given virtual address space.
Takes in the process control block, start address aligned to page size, number of pages being mapped,
an array of pointers to framephy_structs containing the mapped frames and a pointer to an vm_rg_struct
that returns the mapped region and the real mapped fp.
Iterates through each page and obtains the page table entry for each address. If successful,
maps the frame to the address and tracks the page for future replacement activities (if needed).
Returns 0 on success and -1 if it fails to allocate PTE. */
int vmap_page_range(struct pcb_t *caller, int addr, int pgnum, struct framephy_struct *frames, struct vm_rg_struct *ret_rg)
{
  // Initialize variables
  struct framephy_struct *fpit = malloc(sizeof(struct framephy_struct));
  int pgit = 0;
  int pgn = PAGING_PGN(addr);

  // Set the start and end of the return mapped region to the provided address
  ret_rg->rg_end = ret_rg->rg_start = addr;

  fpit->fp_next = frames;

  // Map range of frame to address space
  while (pgit < pgnum)
  {
    uint32_t *pte = get_pte_for_addr(caller->mm, addr + pgit * PAGING_PAGESZ, /*create*/ 1);
    if (!pte)
    {
      // Couldn't allocate PTE, so return error
      return -1;
    }
    uint32_t pte_val = FP_TO_PFN(fpit->fp_paddr) | PAGING_PRESENT | PAGING_WRITABLE | PAGING_USER;
    *pte = pte_val;

    // Update the end of the return mapped region
    ret_rg->rg_end += PAGING_PAGESZ;

    fpit = fpit->fp_next;
    pgit++;

    /* Tracking for later page replacement activities (if needed)
    Enqueue new usage page */
    enlist_pgn_node(&caller->mm->fifo_pgn, pgn + pgit);
  }

  // Return success
  return 0;
}

/*
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */

/* Function that allocates a range of physical memory pages for a process.
Takes in a process control block, a request for the number of pages, and a pointer to an array of pointers to framephy_structs.
Iterates through the request page number and calls MEMPHY_get_freefp function to get free pages.
Returns 0 on success. */
int alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst)
{
  int pgit, fpn;

  // Iterate through the requested page number
  for (pgit = 0; pgit < req_pgnum; pgit++)
  {
    // Call MEMPHY_get_freefp function to get free frames
    if (MEMPHY_get_freefp(caller->mram, &fpn) == 0)
    {
    }
    else
    { // If we cannot obtain enough frames
      // Return error code or handle error accordingly
    }
  }

  return 0; // Return success
}

/*
 * vm_map_ram - do the mapping all vm are to ram storage device
 * @caller    : caller
 * @astart    : vm area start
 * @aend      : vm area end
 * @mapstart  : start mapping point
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
/* Function that maps a range of physical memory pages to a given virtual address space.
Takes in the process control block, start and end addresses of the address space,
starting virtual address to be mapped, number of pages to be mapped incrementally,
and a pointer to a vm_rg_struct that returns the mapped region and the real mapped fp.

The function calls alloc_pages_range to attempt allocation of enough physical memory frames to map the given address space.
If successful, it then calls vmap_page_range to map each frame to the corresponding virtual addresses and returns success.
Returns -1 if memory allocation fails or the process runs out of memory. */
int vm_map_ram(struct pcb_t *caller, int astart, int aend, int mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
  // Initialize variables
  struct framephy_struct *frm_lst = NULL;
  int ret_alloc;

  /*@bksysnet: author provides a feasible solution of getting frames
   *FATAL logic in here, wrong behaviour if we have not enough page
   *i.e. we request 1000 frames meanwhile our RAM has size of 3 frames
   *Don't try to perform that case in this simple work, it will result
   *in endless procedure of swap-off to get frame and we have not provide
   *duplicate control mechanism, keep it simple
   */

  // Attempt to allocate enough physical memory frames to map the given address space incrementally.
  // If successful, store the frames in frm_lst and continue with mapping.
  ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst);

  if (ret_alloc < 0 && ret_alloc != -3000)
    return -1;

  // Out of memory
  if (ret_alloc == -3000)
  {
#ifdef MMDBG
    printf("OOM: vm_map_ram out of memory \n");
#endif
    return -1;
  }

  /* it leaves the case of memory is enough but half in ram, half in swap
   * do the swaping all to swapper to get the all in ram */

  // Map each physical memory frame to its corresponding virtual address.
  vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);

  // Return success
  return 0;
}

/* Swap copy content page from source frame to destination frame
 * @mpsrc  : source memphy
 * @srcfpn : source physical page number (FPN)
 * @mpdst  : destination memphy
 * @dstfpn : destination physical page number (FPN)
 **/
/* Function that swaps a page from source physical memory to destination physical memory.
Takes in a pointer to the source physical memory structure, page number of the page to be swapped,
a pointer to the destination physical memory structure, and page number of the destination page.

The function uses a loop to iterate over each cell (page size) of the source and destination pages,
and reads each byte of data from the source page and writes it to the corresponding cell of the destination page.

Returns success after swapping the entire page. */
int __swap_cp_page(struct memphy_struct *mpsrc, int srcfpn,
                   struct memphy_struct *mpdst, int dstfpn)
{
  // Initialize variables
  int cellidx;
  int addrsrc, addrdst;

  // Iterate over each cell of the page
  for (cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;

    BYTE data;

    // Read each byte of data from the source page
    MEMPHY_read(mpsrc, addrsrc, &data);

    // Write the data to the corresponding cell of the destination page
    MEMPHY_write(mpdst, addrdst, data);
  }

  // Return success
  return 0;
}

/*
 *Initialize a empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
/* Function that initializes a memory management structure for a process.
Takes in a pointer to the memory management structure and a pointer to the process control block.

The function allocates memory for a virtual memory area structure and initializes it with default values.
It also allocates memory for a page directory for the process.

Returns success after initializing the memory management structure. */
int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
  // Allocate memory for a virtual memory area structure
  struct vm_area_struct *vma = malloc(sizeof(struct vm_area_struct));

  // Allocate memory for a page directory for the process
  mm->pgd = malloc(PAGING_MAX_PGN * sizeof(uint32_t));

  /* By default the owner comes with at least one vma */

  // Initialize the virtual memory area structure with default values
  vma->vm_id = 1;
  vma->vm_start = 0;
  vma->vm_end = vma->vm_start;
  vma->sbrk = vma->vm_start;

  // Initialize a virtual memory range structure and add it to the virtual memory free range list
  struct vm_rg_struct *first_rg = init_vm_rg(vma->vm_start, vma->vm_end);
  enlist_vm_rg_node(&vma->vm_freerg_list, first_rg);

  // Set the virtual memory area structure fields
  vma->vm_next = NULL;
  vma->vm_mm = mm; // point back to the memory management structure

  // Set the process's mmap to the virtual memory area
  mm->mmap = vma;

  // Return success
  return 0;
}

/* Function that initializes a virtual memory range structure with the given start and end values.
Allocates memory for the structure, sets its fields and returns the initialized structure. */
struct vm_rg_struct *init_vm_rg(int rg_start, int rg_end)
{
  // Allocate memory for a new virtual memory range structure
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

  // Initialize the virtual memory range node with start and end values
  rgnode->rg_start = rg_start;
  rgnode->rg_end = rg_end;
  rgnode->rg_next = NULL;

  // Return the initialized virtual memory range structure
  return rgnode;
}

/* Function that enlists a virtual memory range node in the virtual memory free range list.
Sets the node's next pointer to the current head of the list and sets the list head to the given node. */
int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode)
{
  // Set the virtual memory range node's next pointer to the current head of the list
  rgnode->rg_next = *rglist;

  // Set the list head to the given node
  *rglist = rgnode;

  // Return success
  return 0;
}

/* Function that enlists a physical page node in the physical page list.
Allocates memory for a new physical page node, sets it's field and returns the initialized node. */
int enlist_pgn_node(struct pgn_t **plist, int pgn)
{
  // Allocate memory for a new physical page node
  struct pgn_t *pnode = malloc(sizeof(struct pgn_t));

  // Initialize the physical page node with the given page number and next pointer
  pnode->pgn = pgn;
  pnode->pg_next = *plist;

  // Set the physical page list head to the new node
  *plist = pnode;

  // Return success
  return 0;
}

/* Function that prints the physical frame list with the frame numbers. */
int print_list_fp(struct framephy_struct *ifp)
{
  struct framephy_struct *fp = ifp;

  printf("print_list_fp: ");

  // Check if the list is null
  if (fp == NULL)
  {
    printf("NULL list\n");
    return -1;
  }
  printf("\n");

  // Print the frame number of each physical frame node in the list
  while (fp != NULL)
  {
    printf("fp[%d]\n", fp->fpn);
    fp = fp->fp_next;
  }
  printf("\n");

  // Return success
  return 0;
}

/* Function that prints the virtual memory range list with start and end values. */
int print_list_rg(struct vm_rg_struct *irg)
{
  struct vm_rg_struct *rg = irg;

  printf("print_list_rg: ");

  // Check if the list is null
  if (rg == NULL)
  {
    printf("NULL list\n");
    return -1;
  }
  printf("\n");

  // Print the start and end value of each virtual memory range node in the list
  while (rg != NULL)
  {
    printf("rg[%ld->%ld]\n", rg->rg_start, rg->rg_end);
    rg = rg->rg_next;
  }
  printf("\n");

  // Return success
  return 0;
}

/* Function that prints the virtual memory area list with start and end values. */
int print_list_vma(struct vm_area_struct *ivma)
{
  struct vm_area_struct *vma = ivma;

  printf("print_list_vma: ");

  // Check if the list is null
  if (vma == NULL)
  {
    printf("NULL list\n");
    return -1;
  }
  printf("\n");

  // Print the start and end value of each virtual memory area node in the list
  while (vma != NULL)
  {
    printf("va[%ld->%ld]\n", vma->vm_start, vma->vm_end);
    vma = vma->vm_next;
  }
  printf("\n");

  // Return success
  return 0;
}

/* Function that prints the physical page list with page numbers. */
int print_list_pgn(struct pgn_t *ip)
{
  printf("print_list_pgn: ");

  // Check if the list is null
  if (ip == NULL)
  {
    printf("NULL list\n");
    return -1;
  }
  printf("\n");

  // Print the page number of each physical page node in the list
  while (ip != NULL)
  {
    printf("va[%d]-\n", ip->pgn);
    ip = ip->pg_next;
  }
  printf("n");

  // Return success
  return 0;
}

/* Function that prints the page table for given start and end addresses of a process. */
int print_pgtbl(struct pcb_t *caller, uint32_t start, uint32_t end)
{
  int pgn_start, pgn_end;
  int pgit;

  // If end is not provided, set it to the end of the current virtual memory area
  if (end == -1)
  {
    pgn_start = 0;
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, 0);
    end = cur_vma->vm_end;
  }

  // Convert start and end address into corresponding page numbers
  pgn_start = PAGING_PGN(start);
  pgn_end = PAGING_PGN(end);

  printf("print_pgtbl: %d - %d", start, end);

  // Check if the caller is null
  if (caller == NULL)
  {
    printf("NULL caller\n");
    return -1;
  }
  printf("\n");

  // Print the page table entries for each page number in the range
  for (pgit = pgn_start; pgit < pgn_end; pgit++)
  {
    printf("%08ld: %08x\n", pgit * sizeof(uint32_t), caller->mm->pgd[pgit]);
  }

  // Return success
  return 0;
}

// #endif
