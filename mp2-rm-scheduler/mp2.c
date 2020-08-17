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
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <asm/spinlock.h>
#include "mp2_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Taipeng Liu");
MODULE_DESCRIPTION("CS-423 MP2");

#define DEBUG 1
#define MP2_DIR "mp2"
#define STATUS_FILE "status"
#define BUFFER_SIZE 100

/* Define new structure reg_info.*/
struct reg_info {
   pid_t pid;
   unsigned long period_ms;
   unsigned long compute_time_ms;
   unsigned long deadline_jiff;
   int state; //0:READY 1:RUNNING 2:SLEEPING
   struct task_struct* linux_task;
   struct timer_list wakeup_timer;
   struct list_head reg_head;
};

/* Global variables */
struct proc_dir_entry *mp2_dir;
struct proc_dir_entry *status_file;
struct list_head reg_list; //Critical Section
struct reg_info *cur_reg;  //Points to current reg_info
static spinlock_t splock;  //Lock of CS 
struct task_struct *disp_thread;
static struct kmem_cache *my_cache;

/* SLAB ALLOCATOR OPERATIONS */
static int init_my_cache(void) {
   my_cache = KMEM_CACHE(reg_info, SLAB_HWCACHE_ALIGN);
   return 0;
}

static int del_my_cache(void) {
   if (my_cache) kmem_cache_destroy(my_cache);
   return 0;
}

/* LIST OPERATIONS */
int sprint_list(char *buf) {
   int len = 0;
   struct list_head *ptr;
   struct reg_info *entry;
   
   spin_lock_irq(&splock);
   for (ptr = reg_list.next; ptr != &reg_list; ptr = ptr->next) {
	entry = list_entry(ptr, struct reg_info, reg_head);
	len += sprintf(buf + len, "%d: %ld %ld\n", entry->pid, entry->period_ms, entry->compute_time_ms);
   }
   spin_unlock_irq(&splock);

   return len;
}

struct reg_info* find_in_list(pid_t pid) {
   struct list_head *ptr;
   struct reg_info *entry = NULL;
   struct reg_info *found = NULL;

   spin_lock_irq(&splock);
   for (ptr = reg_list.next; ptr != &reg_list; ptr = ptr->next) {
	entry = list_entry(ptr, struct reg_info, reg_head);
	if (entry->pid == pid) {
	   found = entry;
	   break;
	}
   }
   spin_unlock_irq(&splock);

   return found;
}

struct reg_info* get_highest_priority_entry(void) {
   struct list_head *ptr;
   struct reg_info *highest_priority_entry = NULL;
   struct reg_info *temp_entry;

   spin_lock_irq(&splock);
   for (ptr = reg_list.next; ptr != &reg_list; ptr = ptr->next) {
	temp_entry = list_entry(ptr, struct reg_info, reg_head);
	if (temp_entry->state != 0)
		continue;

	if (highest_priority_entry) {
		if (temp_entry->period_ms < highest_priority_entry->period_ms) {
			highest_priority_entry = temp_entry;
		}
	} else {
		highest_priority_entry = temp_entry;
	}
   }
   spin_unlock_irq(&splock);

   return highest_priority_entry;
}

int cleanup_list(void) {
   struct list_head *ptr;
   struct reg_info *entry;

   spin_lock_irq(&splock);
   ptr = reg_list.next;
   for (; ptr != &reg_list;) {
	   entry = list_entry(ptr, struct reg_info, reg_head);
	   ptr = ptr->next;

	   list_del(&entry->reg_head);
	   del_timer_sync(&(entry->wakeup_timer)); 
	   //kfree(entry);
	   kmem_cache_free(my_cache, entry);
   }
   spin_unlock_irq(&splock);

   return 0;
}

/* ADMISSION CONTROL FUNCTION */
int can_be_scheduled(unsigned long period, unsigned long compute_time) {
   unsigned long sum = 0;
   struct list_head *ptr;
   struct reg_info *entry = NULL;

   spin_lock_irq(&splock);
   for (ptr = reg_list.next; ptr != &reg_list; ptr = ptr->next) {
	entry = list_entry(ptr, struct reg_info, reg_head);
	sum += (1000 * (entry->compute_time_ms)) / entry->period_ms;
   }
   spin_unlock_irq(&splock);

   sum += (1000 * compute_time) / period;

   if (sum > 693)
	return 0;
   else 
	return 1;
}


/* DISPATCHING KERNEL THREAD FUNCTION : handle context switch*/
static int dispatch(void *data) {

   printk(KERN_DEBUG "DISPATCHING THREAD: INITIALIZED\n\n");
   //Sleep and trigger context switch
   set_current_state(TASK_INTERRUPTIBLE);
   schedule();

   while(!kthread_should_stop()) {
   	struct sched_param old_sparam;
   	struct sched_param new_sparam;
   	struct reg_info *next_reg;
   
	next_reg = NULL;
   	old_sparam.sched_priority = 0;
   	new_sparam.sched_priority = 99;

	printk(KERN_DEBUG "DISPATCHING THREAD: WAKEUP\n");

   	//Find the READY task (state = 0) with highest priority (shortest period)
   	next_reg = get_highest_priority_entry();

   	//Preempt the currently running task (if any) (use scheduler API)
   	if (cur_reg) {
   	   //Set the state of old task to READY (state = 0) if state == 1 (RUNNING)
	   printk(KERN_DEBUG "DISPATCHING THREAD: CURRENT TASK IS PID %d\n", cur_reg->pid);
	   spin_lock_irq(&splock);
	   if (cur_reg->state == 1) {
		cur_reg->state = 0;
	   }
	   spin_unlock_irq(&splock);

	   if (cur_reg->linux_task) sched_setscheduler(cur_reg->linux_task, SCHED_NORMAL, &old_sparam);
	} else {
   	   printk(KERN_DEBUG "DISPATCHING THREAD: NO CURRENT RUNNING TASK\n");
	}
   	//Context switch to the found task (if found) (use scheduler API)
   	if (next_reg) {
   	   //Set the state of new running task to RUNNING (state = 1)
	   printk(KERN_DEBUG "DISPATCHING THREAD: NEXT TASK IS PID %d\n", next_reg->pid);
	   spin_lock_irq(&splock);
	   next_reg->state = 1;
	   spin_unlock_irq(&splock);

	   if (next_reg->linux_task) {
	      wake_up_process(next_reg->linux_task);
	      sched_setscheduler(next_reg->linux_task, SCHED_FIFO, &new_sparam);
   	   }
	} else {
   	   printk(KERN_DEBUG "DISPATCHING THREAD: NO NEXT TASK TO RUN\n");
	}

   	cur_reg = next_reg;

	//Sleep and trigger context switch
	set_current_state(TASK_INTERRUPTIBLE);
   	printk(KERN_DEBUG "DISPATCHING THREAD: SLEEPING\n\n");
	schedule();
   }
   printk(KERN_DEBUG "DISPATCHING THREAD: EXIT\n\n");

   return 0;
}

/* WAKEUP TIMER CALLBACK FUNCTION*/
void wakeup_timer_callback(unsigned long pid) {
   struct reg_info *this_reg = find_in_list(pid);

   printk(KERN_DEBUG "TIMER: EXPIRES AT PID = %lu\n", pid);

   //Change the state of the task to 0 (READY)
   if (this_reg != NULL) {
   	spin_lock_irq(&splock);
	this_reg->state = 0;
	this_reg->deadline_jiff = jiffies;
   	spin_unlock_irq(&splock);
   }

   //Wakeup dispatching thread function
   wake_up_process(disp_thread);
}


/* LIST OPERATIONS */
int add_to_list(pid_t pid, unsigned long period, unsigned long compute_time){
   struct reg_info *new_info;

   //find if it exists
   if (find_in_list(pid)) {
	return -1;
   }

   /* New Registration */
   //new_info = kmalloc(sizeof(struct reg_info), GFP_KERNEL);
   new_info = kmem_cache_alloc(my_cache, GFP_KERNEL);
   //Constant
   new_info->pid = pid;
   new_info->period_ms = period;
   new_info->compute_time_ms = compute_time;
   INIT_LIST_HEAD( &(new_info->reg_head));
   //Valuable
   new_info->deadline_jiff = jiffies;
   new_info->state = 2; //Sleeping
   new_info->linux_task = find_task_by_pid(pid);
   init_timer(&new_info->wakeup_timer);
   new_info->wakeup_timer.function = wakeup_timer_callback;
   new_info->wakeup_timer.data = (unsigned long)pid;
   new_info->wakeup_timer.expires = new_info->deadline_jiff;
 
   spin_lock_irq(&splock);
   list_add(&(new_info->reg_head), &reg_list);
   //if (cur_reg == NULL) cur_reg = new_info; //TODO
   spin_unlock_irq(&splock);

   return 0;
}

int delete_from_list(pid_t pid) {
   struct reg_info *entry = find_in_list(pid);

   if (entry) {
   	spin_lock_irq(&splock);
	if (cur_reg != NULL && cur_reg->pid == pid)
		cur_reg = NULL;
	
	list_del(&entry->reg_head);
	del_timer_sync(&(entry->wakeup_timer));
	//free the memory
	kmem_cache_free(my_cache, entry);

	//TODO need to wakeup disp_thread?
	//wake_up_process(disp_thread);
   	spin_unlock_irq(&splock);
   } else {
	printk(KERN_DEBUG "NO ENTRY WITH PID = %d\n", pid);
	return -1;
   }

   return 0;
}


/* WRITE CALLBACK FUNCTION*/
static ssize_t write_proc_callback(struct file *file, const char __user *ubuf, size_t ubuf_size, loff_t *pos){
   int c;
   char buf[BUFFER_SIZE];
   char m;//msg type, "R", "Y", "D"
   int pid = 0;
   unsigned long period = 0;
   unsigned long compute_time = 0;
   unsigned long next_release_time = 0;
   struct reg_info *entry = NULL;

   printk(KERN_DEBUG ">>>HANDLE WRITE>>>\n");
   memset(buf, 0, BUFFER_SIZE);

   if(*pos > 0 || ubuf_size > BUFFER_SIZE)
	return -EFAULT;

   if(copy_from_user(buf, ubuf, ubuf_size))
	return -EFAULT;

   sscanf(buf, "%c%*[,] %d%*[,] %lu%*[,] %lu", &m, &pid, &period, &compute_time);

   switch(m) {
	case 'R'://REGISTER
	   //admission control
	   if (can_be_scheduled(period, compute_time)) {
	      //Add new_reg to reg_list
	      printk(KERN_DEBUG "NEW REGISTARTION: PID = %d, PERIOD = %lu COMPUTE TIME = %lu\n", pid, period, compute_time);
	      add_to_list(pid, period, compute_time);
	   } else {
	      printk(KERN_DEBUG "REGISTARTION DENIED: PID = %d, PERIOD = %lu COMPUTE TIME = %lu\n", pid, period, compute_time);
	   }

	   break;
	case 'Y'://YIELD
	   //Find the calling reg_info entry
	   printk(KERN_DEBUG "RECEIVE YIELD: PID = %d\n", pid);
	   entry = find_in_list(pid);

	   if (entry) {
   		spin_lock_irq(&splock);

	   	//Set reg state to 2 (SLEEPING)
		entry->state = 2;
	   	//Calculate the time when next period begins
	   	next_release_time = entry->deadline_jiff + msecs_to_jiffies(entry->period_ms);
	   	//current deadline has passed, go to next period TODO or we keep this task ready????
	   	while(next_release_time < jiffies) {
		    next_release_time += msecs_to_jiffies(entry->period_ms);
	   	}
	   	//Setup wakeup timer
	   	printk(KERN_DEBUG "ADD TIMER\n");
   	   	entry->wakeup_timer.expires = next_release_time;
	   	add_timer(&(entry->wakeup_timer));
   		
		spin_unlock_irq(&splock);

	        printk(KERN_DEBUG "WAKEUP DISPATCHING THREAD\n");
	   	//Wake up dispatching thread
	   	wake_up_process(disp_thread);

	        //Put application to sleep (Linux API)
	   	set_task_state(entry->linux_task, TASK_UNINTERRUPTIBLE);
		schedule();
	   } else {
	        printk(KERN_DEBUG "NO ENTRY WITH PID = %d\n", pid);
	   }

	   break;
	case 'D'://DE-REGISTER
	   //Delete from reg_list
	   printk(KERN_DEBUG "RECEIVE DELETE: PID = %d\n", pid);

	   delete_from_list(pid);

	   break;
   }
   /* Reture number of bytes written. */
   c = strlen(buf);
   *pos = c;

   printk(KERN_DEBUG "<<<WRITE FINISH<<<\n\n");

   return c;
}

/* READ CALLBACK FUNCTION: print the registration list.*/
static ssize_t read_proc_callback(struct file *file, char __user *ubuf, size_t ubuf_size, loff_t *pos){
   char buf[BUFFER_SIZE];
   int len = 0;

   printk( KERN_DEBUG ">>>HANDLE READ>>>\n");

   if (*pos > 0 || ubuf_size < BUFFER_SIZE)
   	return 0;
   
   //print list to buffer
   len = sprint_list(buf);

   //copy data into userbuffer
   if(copy_to_user(ubuf, buf, len))
	return -EFAULT;
   *pos = len;
   printk(KERN_DEBUG "<<<READ FINISH<<<\n\n");

   return len;
}

static struct file_operations status_ops = {
   .owner = THIS_MODULE,
   .read  = read_proc_callback,
   .write = write_proc_callback,
};

/* mp2_init - Called when module is loaded */
int __init mp2_init(void)
{
   #ifdef DEBUG
   printk(KERN_ALERT "MP2 MODULE LOADING\n");
   #endif

   /* Create proc filesystem entries */
   mp2_dir     = proc_mkdir(MP2_DIR, NULL);
   status_file = proc_create(STATUS_FILE, 0666, mp2_dir, &status_ops);
   if (status_file == NULL) 
   {
	printk(KERN_ALERT "Error: Could not initialize %s\n", STATUS_FILE);
	return -ENOMEM;
   }
  
   /* Initialize slab allocator */
   init_my_cache();
   
   /* Initialize head of linked list "reg_list"*/
   INIT_LIST_HEAD(&reg_list);

   /* Initialize spin locks */
   spin_lock_init(&splock);

   /* init cur_reg */
   cur_reg = NULL;

   /* Create dispatching thread */
   disp_thread = kthread_create(dispatch, NULL, "dispatching thread");
   wake_up_process(disp_thread);

   printk(KERN_ALERT "MP2 MODULE LOADED\n");
   return 0;   
}

/* mp2_exit - Called when module is unloaded */
void __exit mp2_exit(void)
{
   #ifdef DEBUG
   printk(KERN_ALERT "MP2 MODULE UNLOADING\n");
   #endif

   /* Stop dispatching thread */
   kthread_stop(disp_thread);

   /* Clean up all memory of reg_list */
   cleanup_list();
   
   /* Remove slab allocator */
   del_my_cache();

   /* Remove file entries */
   proc_remove(status_file);
   proc_remove(mp2_dir);

   printk(KERN_ALERT "MP2 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp2_init);
module_exit(mp2_exit);
