/* This is myMmap.h */

#include <linux/mm.h>
#include <linux/vmalloc.h>

#define MY_PAGE_SIZE 4*1024

void *alloc_mmap_pages(int npages) {
	int i;
	char *mem = vzalloc(MY_PAGE_SIZE * npages);

	if (!mem)
		return mem;

	for(i = 0; i < npages * MY_PAGE_SIZE; i += MY_PAGE_SIZE) {
		SetPageReserved(vmalloc_to_page((void *)((unsigned long)mem) + i));
	}

	return mem;
}

void free_mmap_pages(void *mem, int npages) {
	int i;
	for (i = 0; i < npages * MY_PAGE_SIZE; i += MY_PAGE_SIZE) {
		ClearPageReserved(vmalloc_to_page((void *)((unsigned long)mem) + i));
	}

	vfree(mem);
}

