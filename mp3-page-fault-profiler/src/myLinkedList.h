/* This is myLinkedList.h */

#include <linux/list.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>

/* GLOBAL VARIABLES FROM mp3.c */

/* GLOBAL VARIABLES */
struct list_head reg_list;
unsigned int total_len;

/* FUNCTION FROM mp3_given.h */
extern struct task_struct *find_task_by_pid(unsigned int nr);

/* Define structure of the node.*/
struct reg_info {
   pid_t pid;
   unsigned long cpuUsage; // unit: /1000
   unsigned long maj_flt;
   unsigned long min_flt;
   struct task_struct* linux_task;
   struct list_head reg_head;
};


/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////READ OPERATIONS/////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
unsigned int get_list_length(void) {
   return total_len;
}

int list_is_empty(void) {
   if (total_len) 
	return 0;
   else 
	return 1;
}

int print_list_to(char *buf) {
   int len = 0;
   struct list_head *ptr;
   struct reg_info *entry;
   
   for (ptr = reg_list.next; ptr != &reg_list; ptr = ptr->next) {
	entry = list_entry(ptr, struct reg_info, reg_head);
	//len += sprintf(buf + len, "%d\n", entry->pid);
	//DEBUG
	len += sprintf(buf + len, "PID = %d, CPU = %lu/1000, MAJ_FLT = %lu, MIN_FLT = %lu\n", entry->pid, entry->cpuUsage, entry->maj_flt, entry->min_flt);
   }

   return len;
}

struct reg_info* find_in_list(pid_t pid) {
   struct list_head *ptr;
   struct reg_info *entry = NULL;
   struct reg_info *found = NULL;

   for (ptr = reg_list.next; ptr != &reg_list; ptr = ptr->next) {
	entry = list_entry(ptr, struct reg_info, reg_head);
	if (entry->pid == pid) {
	   found = entry;
	   break;
	}
   }

   return found;
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////WRITE OPERATIONS/////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
int create_list(void) {
   INIT_LIST_HEAD(&reg_list);
   total_len = 0;
   return 0;
}

int add_to_list(pid_t pid){
   struct reg_info *new_info;
   struct task_struct *task;

   //Check if it is a linux task
   if ((task = find_task_by_pid(pid)) == NULL)
	   return -1;

   //Check if it has already registered
   if (find_in_list(pid))
	   return -1;

   //Memory allocation
   new_info = kmalloc(sizeof(struct reg_info), GFP_KERNEL);

   //Initialize
   new_info->pid = pid;
   new_info->cpuUsage = 0;
   new_info->maj_flt = 0;
   new_info->min_flt = 0;
   new_info->linux_task = task;
   INIT_LIST_HEAD( &(new_info->reg_head));

   //Add 
   list_add(&(new_info->reg_head), &reg_list);
   total_len += 1;

   return 0;
}

int modify_list_at(pid_t pid, unsigned long cpuUsage, unsigned long maj_flt, unsigned long min_flt) {
   struct reg_info *found;

   found = find_in_list(pid);
   if (found) {
	found->cpuUsage = cpuUsage;
	found->maj_flt = maj_flt;
	found->min_flt = min_flt;
	return 0;
   }

   return -1;
}

int delete_from_list(pid_t pid) {
   struct reg_info *entry;
   
   if (list_is_empty()) return -1;

   entry = find_in_list(pid);

   if (entry) {
	list_del(&entry->reg_head);
	total_len -= 1;

	kfree(entry);
   } else {
	printk(KERN_DEBUG "NO ENTRY WITH PID = %d\n", pid);
	return -1;
   }

   return 0;
}

int cleanup_list(void) {
   struct list_head *ptr;
   struct reg_info *entry;

   for (ptr = reg_list.next; ptr != &reg_list;) {
	   entry = list_entry(ptr, struct reg_info, reg_head);
	   ptr = ptr->next;

	   list_del(&entry->reg_head);
	   
	   kfree(entry);
   }

   return 0;
}
