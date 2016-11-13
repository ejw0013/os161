#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/spl.h>
#include <machine/tlb.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */
/*
struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	as->pages = NULL;
	as->regions = NULL;
	as->stack = NULL;
	as->heap = NULL;
	as->heap_end = (vaddr_t)0;
	as->heap_start = (vaddr_t)0;

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;
	
	kprintf("1.1\n");

	newas = as_create();
	kprintf("1.2\n");

	if (newas==NULL) {
		kprintf("1.3\n");
		return ENOMEM;
	}
	
		kprintf("1.4\n");
	struct regionlist* itr, *newitr, *tmp;

		kprintf("1.5\n");
	itr = old->regions;

		kprintf("1.6\n");
	while(itr!=NULL) {
		kprintf("1.7\n");
		if(newas->regions == NULL) {
		kprintf("1.8\n");
			newas->regions = (struct regionlist*) kmalloc(sizeof(struct regionlist));
			newas->regions->next = NULL;
			newitr = newas->regions;
		} else {
		kprintf("1.9\n");
			for(tmp=newas->regions; tmp->next!=NULL; tmp=tmp->next);
			newitr = (struct regionlist*) kmalloc(sizeof(struct regionlist));
			tmp->next = newitr;
		}
		kprintf("1.10\n");

		newitr->vbase = itr->vbase;
		newitr->pbase = itr->pbase;
		newitr->npages = itr->npages;
		newitr->permissions = itr->permissions;
		newitr->next = NULL;
		itr = itr->next;
	}
	
		kprintf("1.11\n");
	if(as_prepare_load(newas)) {
		kprintf("1.12\n");
		as_destroy(newas);
		return EINVAL;
	}

		kprintf("1.13\n");
	struct page_table_entry* iterate1 = old->pages;
	struct page_table_entry* iterate2 = newas->pages;

		kprintf("1.14\n");
	while(iterate1 != NULL) {
		kprintf("1.15\n");
		memmove((void*) PADDR_TO_KVADDR(iterate2->paddr),
				(const void*)PADDR_TO_KVADDR(iterate1->paddr), PAGE_SIZE);
		kprintf("1.16\n");
		iterate1 = iterate1->next;
		iterate2 = iterate2->next;
	}
	
		kprintf("1.17\n");
	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	if (as != NULL) {
		struct regionlist* reglst = as->regions;
		struct regionlist* temp;
		while(reglst) {
			temp = reglst;
			reglst = reglst->next;
			kfree(temp);
		}

		struct page_table_entry* pte=as->pages;
		struct page_table_entry* pagetemp;

		while (pte != NULL) {
			pagetemp = pte;
			pte = pte->next;
			
			free_kpages(pagetemp->paddr);
			kfree(pagetemp);
		}
		
	}	
	kfree(as);
}

void
as_activate(struct addrspace *as)
{
	int i, spl;
	(void) as;
	
	spl = splhigh();

	for (i = 0; i < NUM_TLB; i++) {
		TLB_Write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}
*/
/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
/*int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	size_t npages;

	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;
	npages = sz / PAGE_SIZE;

	struct regionlist* end;
	if (as->regions != NULL) {
		end = as->regions->last;
	}
	
	if (as->regions == NULL) {
		as->regions = (struct regionlist*) kmalloc(sizeof(struct regionlist));
		as->regions->next = NULL;
		as->regions->last = as->regions;
		end = as->regions;
	} else {
		end = as->regions->last;
		end->next = (struct regionlist*) kmalloc(sizeof(struct regionlist));
		end = end->next;
		end->next = NULL;
		as->regions->last = end;
	}

	end->vbase = vaddr;
	end->npages = npages;
	end->pbase = 0;
	end->permissions = 7 & (readable | writeable | executable);

	return 0;
}

int
as_prepare_load(struct addrspace *as)
{
	paddr_t paddr;
	vaddr_t vaddr;

	struct regionlist* regionlst;
	struct page_table_entry* pages;

	regionlst = as->regions;
	
	size_t i;
	while (regionlst != NULL) {
		vaddr = regionlst->vbase;
		for (i = 0; i < regionlst->npages; i++) {
			if (as->pages==NULL) {
				
				as->pages = (struct page_table_entry*)kmalloc(sizeof(struct page_table_entry));
				as->pages->vaddr = vaddr;
				as->pages->permissions = regionlst->permissions;
				as->pages->next = NULL;
				paddr = alloc_kpages(1);
				if (paddr == 0) {
					return ENOMEM;
				}
				as->pages->paddr = paddr;
			} else {
				for (pages = as->pages; pages->next != NULL; pages = pages->next);
				pages->next = (struct page_table_entry*) kmalloc(sizeof(struct page_table_entry));
				pages->next->vaddr = vaddr;
				pages->next->permissions = regionlst->permissions;
				pages->next->next = NULL;
				paddr = alloc_kpages(1);
				if (paddr == 0) {
					return ENOMEM;
				}
				pages->next->paddr = paddr;
			}
			vaddr += PAGE_SIZE;
		}

		regionlst = regionlst->next;
	}
	
	struct page_table_entry* heap_page = (struct page_table_entry*)kmalloc(sizeof(struct page_table_entry));
	pages->next = heap_page;
	heap_page->next = NULL;

	paddr = alloc_kpages(1);
	if (paddr == 0)  {
		return ENOMEM;
	}

	heap_page->paddr = paddr;
	heap_page->vaddr = vaddr;

	as->heap_start = as->heap_end = vaddr;
	as->heap = heap_page;
	
	assert(as->heap_start != 0);
	assert(as->heap_end != 0);


	return 0;
}
*/
/*
int
as_complete_load(struct addrspace *as)
{

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{

	(void)as;

	/* Initial user-level stack pointer 
	*stackptr = USERSTACK;
	
	return 0;
}*/

