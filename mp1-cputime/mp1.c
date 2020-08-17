#define LINUX

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <asm/spinlock.h>
#include "mp1_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Taipeng Liu");
MODULE_DESCRIPTION("CS-423 MP1");

#define DEBUG 1
#define MP1_DIR "mp1"
#define STATUS_FILE "status"
#define BUFFER_SIZE 100
#define WQ_NAME "MY_WQ"

/* Define new structure reg_info.*/
struct reg_info {
   pid_t PID;
   unsigned long CPU_time;
   struct list_head reg_head;
};

/* Global variables */
struct proc_dir_entry *mp1_dir;
struct proc_dir_entry *status_file;
struct list_head reg_list;
static struct timer_list update_timer;
static struct workqueue_struct *update_wq;
static spinlock_t splock;

/* WORK FUNCTION */
void update_reg_list(struct work_struct *work) {
   struct list_head *ptr = reg_list.next;
   struct reg_info *entry;

   //printk(KERN_DEBUG "START UPDATING REG_LIST\n");
   //Traverse the list and update each value
   spin_lock_irq(&splock);
   for (; ptr != &reg_list;) {
	   entry = list_entry(ptr, struct reg_info, reg_head);
	   if (get_cpu_use(entry->PID, &(entry->CPU_time)) != 0) {
		   printk(KERN_DEBUG "(PID = %d)Process may finish or exit\n", entry->PID);
		   ptr = ptr->next;
		   list_del(&entry->reg_head);
		   kfree(entry);
	   } else {
		   //printk(KERN_DEBUG "UPDATE: %d:%ld\n", entry->PID, entry->CPU_time);
	   	   ptr = ptr->next;
	   }
   }
   spin_unlock_irq(&splock);

   /* Stop timer if reg_list is empty. */
   if (list_empty(&reg_list)) {
	   del_timer(&update_timer);
   }

   kfree(work);
}

/* Timer callback function. Push a work onto workqueue per 5 sec*/
void update_timer_callback(unsigned long data) {
	/* Schedule work to workqueue */
	struct work_struct *new_work = kmalloc(sizeof(struct work_struct), GFP_KERNEL);

	//printk(KERN_DEBUG "TIMER EXPIRES...\n");
	INIT_WORK(new_work, update_reg_list);
	queue_work(update_wq, new_work);

	update_timer.expires = jiffies + msecs_to_jiffies(5000);
	add_timer(&update_timer);
}


/* callback function of write */
static ssize_t write_proc_callback(struct file *file, const char __user *ubuf, size_t ubuf_size, loff_t *pos){
   int num, c, pid;
   char buf[BUFFER_SIZE];
   struct reg_info *new_info;

   printk(KERN_DEBUG "Handle write\n");

   if(*pos > 0 || ubuf_size > BUFFER_SIZE)
	return -EFAULT;
   if(copy_from_user(buf, ubuf, ubuf_size))
	return -EFAULT;
	
   num = sscanf(buf, "%d", &pid);
   if(num != 1)
	return -EFAULT;

   new_info = kmalloc(sizeof(struct reg_info), GFP_KERNEL);
   new_info->PID = pid;
   if (get_cpu_use(pid, &(new_info->CPU_time)) != 0) {
	   printk(KERN_DEBUG "Error in get_cpu_use\n");
	   kfree(new_info);
	   return -1;
   }
   INIT_LIST_HEAD( &(new_info->reg_head));

   printk(KERN_DEBUG "Write: %d: %ld\n", new_info->PID, new_info->CPU_time);

   /* Setup timer right before adding the first process to reg_list. */
   spin_lock_irq(&splock);
   if (list_empty(&reg_list)) {
	   setup_timer(&update_timer, update_timer_callback, 0);
	   mod_timer(&update_timer, jiffies + msecs_to_jiffies(5000));
   }

   //TODO May write same PID multiple times
   list_add(&(new_info->reg_head), &reg_list);
   spin_unlock_irq(&splock);

   /* Reture number of bytes written. */
   c = strlen(buf);
   *pos = c;
   return c;
}

/* Callback function of read. The function copy data to 
user buffer from the linked list, reg_info.

Return value:  Positive value means the function return
successfully. Return value 0 when get EOF error or the 
length of user buffer is smaller than the data. Negative 
value means some error exist when copy to user buffer.*/
static ssize_t read_proc_callback(struct file *file, char __user *ubuf, size_t ubuf_size, loff_t *pos){
   char buf[BUFFER_SIZE];
   int len = 0;
   struct list_head *ptr;
   struct reg_info *entry;

   printk( KERN_DEBUG "Handle read\n");

   if (*pos > 0 || ubuf_size < BUFFER_SIZE)
   	return 0;
   
   spin_lock_irq(&splock);
   for (ptr = reg_list.next; ptr != &reg_list; ptr = ptr->next) {
	   entry = list_entry(ptr, struct reg_info, reg_head);
	   len += sprintf(buf + len, "%d: %ld\n", entry->PID, entry->CPU_time);
   }
   spin_unlock_irq(&splock);

   if(copy_to_user(ubuf, buf, len))
	return -EFAULT;
   *pos = len;

   return len;
}


static struct file_operations status_ops = {
   .owner = THIS_MODULE,
   .read  = read_proc_callback,
   .write = write_proc_callback,
};



// mp1_init - Called when module is loaded
int __init mp1_init(void)
{
   #ifdef DEBUG
   printk(KERN_ALERT "MP1 MODULE LOADING\n");
   #endif

   /* Create proc filesystem entries */
   mp1_dir     = proc_mkdir(MP1_DIR, NULL);
   status_file = proc_create(STATUS_FILE, 0666, mp1_dir, &status_ops);
   if (status_file == NULL) 
   {
	printk(KERN_ALERT "Error: Could not initialize %s\n", STATUS_FILE);
	return -ENOMEM;
   }
   
   /* Initialize reg_list*/
   INIT_LIST_HEAD(&reg_list);

   /* Initialize update_wq */
   update_wq = create_workqueue(WQ_NAME);

   /* Initialize spin lock 'splock' */
   spin_lock_init(&splock);

   printk(KERN_ALERT "MP1 MODULE LOADED\n");
   return 0;   
}

// mp1_exit - Called when module is unloaded
void __exit mp1_exit(void)
{
   struct list_head *ptr;
   struct reg_info *entry;

   #ifdef DEBUG
   printk(KERN_ALERT "MP1 MODULE UNLOADING\n");
   #endif

   /* Clean up workqueue */
   flush_workqueue(update_wq);
   destroy_workqueue(update_wq);

   /* Stop timer if reg_list is not empty. */
   if (!list_empty(&reg_list)) {
	   del_timer(&update_timer);
   }

   /* Clean up all memory of reg_list */
   ptr = reg_list.next;
   for (; ptr != &reg_list;) {
	   entry = list_entry(ptr, struct reg_info, reg_head);
	   ptr = ptr->next;
	   list_del(&entry->reg_head);
	   kfree(entry);
   }

   /* Remove file entries */
   proc_remove(status_file);
   proc_remove(mp1_dir);

   printk(KERN_ALERT "MP1 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp1_init);
module_exit(mp1_exit);
