#define LINUX

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/vmalloc.h>

#include "mp3_given.h"
#include "src/myLinkedList.h"
#include "src/myProc.h"
#include "src/myWorkQueue.h"
#include "src/myMmap.h"
#include "src/myCDev.h"
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Taipeng Liu");
MODULE_DESCRIPTION("CS-423 MP3");

#define DEBUG 1
#define MP3_DIR "mp3"
#define STATUS_FILE "status"
#define BUFFER_SIZE 100
#define PAGE_NUM 128

/* Global variables */
struct proc_dir_entry *mp3_dir;
struct proc_dir_entry *status_file;
unsigned long *sharedMem;
int pos;

static struct file_operations status_ops = {
   .owner = THIS_MODULE,
   .read  = read_proc_callback,
   .write = write_proc_callback,
};
/* mp3_init - Called when module is loaded */
int __init mp3_init(void)
{
   #ifdef DEBUG
   printk(KERN_ALERT "MP3 MODULE LOADING\n");
   #endif

   /* Create proc filesystem entries */
   mp3_dir     = proc_mkdir(MP3_DIR, NULL);
   status_file = proc_create(STATUS_FILE, 0666, mp3_dir, &status_ops);
   if (status_file == NULL) 
   {
	printk(KERN_ALERT "Error: Could not initialize %s\n", STATUS_FILE);
	return -ENOMEM;
   }
  
   
   /* Create a linked list */
   create_list();

   /* Allocate shared memory */
   sharedMem = (unsigned long *)alloc_mmap_pages(PAGE_NUM);
   pos = 0;

   /* Initialize my char. device */
   init_my_dev();

   printk(KERN_ALERT "MP3 MODULE LOADED\n");
   return 0;   
}

/* mp3_exit - Called when module is unloaded */
void __exit mp3_exit(void)
{
   #ifdef DEBUG
   printk(KERN_ALERT "MP3 MODULE UNLOADING\n");
   #endif

   /* Delete my device */
   delete_my_dev();

   /* Free shared memory */
   free_mmap_pages(sharedMem, PAGE_NUM);

   /* Clean up a list */
   cleanup_list();
  
   /* Remove file entries */
   proc_remove(status_file);
   proc_remove(mp3_dir);

   printk(KERN_ALERT "MP3 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp3_init);
module_exit(mp3_exit);
