/* This is myCDev.h */

#include <linux/cdev.h>
#include <linux/fs.h>

#define MY_MAJOR 18
#define MY_MAX_MINORS 1
#define MY_PAGE_SIZE 4*1024
#define NPAGE 128
/* GLOBAL VARIABLES */
struct cdev my_dev;

/* GLOBAL VARIABLES FROM mp3.c */
extern unsigned long *sharedMem;

/* -------CALLBACK FUNCTIONS ----- */


static int my_open(struct inode *inode, struct file *flip){return 0;}

static int my_release(struct inode *inode, struct file *flip){return 0;}

static int my_mmap(struct file *fp, struct vm_area_struct *vma){
	int i, ret;
	unsigned long pfn;

	for (i = 0; i < NPAGE; i++) {
		//Get physical address
		pfn = vmalloc_to_pfn((void *)((unsigned long)sharedMem) + i*MY_PAGE_SIZE);

		//Map user's virtual page to pfn
		ret = remap_pfn_range(vma, vma->vm_start + i*MY_PAGE_SIZE, pfn, MY_PAGE_SIZE, vma->vm_page_prot);

		if (ret < 0) {
			printk(KERN_DEBUG "remap: could not map the address area\n");
			return -EIO;
		}
	}

	return 0;
}


/* -------my_dev APIs ------------*/
static struct file_operations my_dev_ops = {
   .open = my_open,
   .release = my_release,
   .mmap = my_mmap,
   .owner = THIS_MODULE,
};


int init_my_dev(void) {
	int err;

	err = register_chrdev_region(MKDEV(MY_MAJOR, 0), MY_MAX_MINORS, "my_device");
	if (err != 0) {
		return err;
	}

	cdev_init(&my_dev, &my_dev_ops);
	cdev_add(&my_dev, MKDEV(MY_MAJOR, 0), 1);
	return 0;
}

int delete_my_dev(void) {
	cdev_del(&my_dev);
	unregister_chrdev_region(MKDEV(MY_MAJOR, 0), MY_MAX_MINORS);
	return 0;
}


