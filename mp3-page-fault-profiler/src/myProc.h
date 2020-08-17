/* This is myProc.h */

#include <linux/proc_fs.h>

#define BUFFER_SIZE 100
/* GLOBAL VARIABLES FROM mp3.c */

/* FUNCTIONS FROM myLinkedList.h */
extern int print_list_to(char *buf);
extern int list_is_empty(void);
extern int add_to_list(pid_t pid);
extern int delete_from_list(pid_t pid);

/* FUNCTIONS FROM myWorkQueue.h */
extern int createMyWq(void);
extern int destroyMyWq(void);
extern int setup_my_timer(void);
extern int delete_my_timer(void);

/* REGISTRATION FUNCTION */
int handleRegistration(pid_t pid) {
	int isFirstEntry = 0;
	
	if (list_is_empty())
		isFirstEntry = 1;

      	add_to_list(pid);

	if (isFirstEntry) {
		//Create a work queue job
		createMyWq();
		setup_my_timer();
	}

	return 0;
}

/* DEGETRISTRATION FUNCTION */
int handleDeregistration(pid_t pid) {
	delete_from_list(pid);

	if (list_is_empty()) {
		//Delete a work queue job
		destroyMyWq();
		delete_my_timer();
	}

	return 0;
}

/* WRITE CALLBACK*/
static ssize_t write_proc_callback(struct file *file, const char __user *ubuf, size_t ubuf_size, loff_t *pos){
   int c;
   char buf[BUFFER_SIZE];
   char type;// 'R' - register, 'U' - unregister
   int pid = 0;

   printk(KERN_DEBUG ">>>HANDLE WRITE>>>\n");
   memset(buf, 0, BUFFER_SIZE);

   if(*pos > 0 || ubuf_size > BUFFER_SIZE)
	return -EFAULT;

   if(copy_from_user(buf, ubuf, ubuf_size))
	return -EFAULT;

   sscanf(buf, "%c%*[,] %d", &type, &pid);

   switch(type) {
	case 'R':
      		printk(KERN_DEBUG "NEW REGISTARTION: PID = %d\n", pid);
		handleRegistration(pid);
	   	break;
	case 'U':
	   	printk(KERN_DEBUG "UNREGISTRATION: PID = %d\n", pid);
		handleDeregistration(pid);
	   	break;
	default:
		printk(KERN_DEBUG "UNRECOGNIZED TYPE %c\n", type);
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
   len = print_list_to(buf);

   //copy data into userbuffer
   if(copy_to_user(ubuf, buf, len))
	return -EFAULT;
   *pos = len;
   printk(KERN_DEBUG "<<<READ FINISH<<<\n\n");

   return len;
}
