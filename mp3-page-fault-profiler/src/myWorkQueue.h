/* This is myWorkQueue.h */

#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>

#define WQ_NAME "my_wq"

/* GLOBAL VARIABLES FROM mp3.c */
extern unsigned long *sharedMem;
extern int pos;
/* GLOBAL VARIABLES */
struct workqueue_struct *my_wq;
struct timer_list my_timer;

/* GLOBAL VARIABLES FROM myLinkedList.h */
extern struct list_head reg_list;

/* MY FUNCTIONS */

int createMyWq(void) {
	my_wq = create_workqueue(WQ_NAME);
	return 0;
}

int destroyMyWq(void) {
	flush_workqueue(my_wq);
	destroy_workqueue(my_wq);
	return 0;
}

/* Work function */
void update_reg_list(struct work_struct *work) {
	struct list_head *ptr = reg_list.next;
	struct reg_info *entry;
	unsigned long maj_flt, min_flt, utime, stime;

	for (; ptr != &reg_list; ){
		entry = list_entry(ptr, struct reg_info, reg_head);
		get_cpu_use(entry->pid, &min_flt, &maj_flt, &utime, &stime);
		
		//TODO check if to aggregate or not
		entry->maj_flt = maj_flt;
		entry->min_flt = min_flt;
		entry->cpuUsage = (1000*(utime + stime));

		//TODO Write to shared memory buffer
		sharedMem[pos++] = (unsigned long)jiffies;
		sharedMem[pos++] = entry->min_flt;
		sharedMem[pos++] = entry->maj_flt;
		sharedMem[pos++] = entry->cpuUsage;

		//DEBUG
		//printk(KERN_DEBUG "Write %lu, %lu, %lu, %lu\n", sharedMem[pos-4], sharedMem[pos-3], sharedMem[pos-2], sharedMem[pos-1]);

		ptr = ptr->next;
	}

	kfree(work);
}

/* Timer callback function. Push a work onto my_wq every 50ms (1s = 20 works) */
void my_timer_callback(unsigned long data) {
	struct work_struct *new_work = kmalloc(sizeof(struct work_struct), GFP_KERNEL);

	INIT_WORK(new_work, update_reg_list);
	queue_work(my_wq, new_work);

	my_timer.expires = jiffies + msecs_to_jiffies(50);
	add_timer(&my_timer);
}

int setup_my_timer(void) {
	init_timer(&my_timer);
	my_timer.function = my_timer_callback;
	my_timer.expires = jiffies + msecs_to_jiffies(50);

	add_timer(&my_timer);
	return 0;
}

int delete_my_timer(void) {
	del_timer_sync(&my_timer);
	return 0;
}
