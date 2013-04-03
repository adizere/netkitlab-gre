#undef __KERNEL__
#define __KERNEL__
 
#undef MODULE
#define MODULE

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>


#define THIS_AUTHOR "Dragos-Adrian Seredinschi <adizere@cpan.org>"
#define THIS_NAME "minos-export-test"
#define THIS_DESCRIPTION "Simple test for the minos module."


extern void add_log_data(char* data);


static int __init export_test_init(void)
{
	int i;
	char message[100];
	for(i=0;i<10;i++)
	{
		sprintf(message, "Log Data Test Message Number::%d",i);
		add_log_data(message);
	}

    printk(KERN_INFO "Export test: starting up.");
    return 0;
}


static void __exit export_test_exit(void)
{
    printk(KERN_INFO "Export test: done.\n");
}

module_init(export_test_init);
module_exit(export_test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(THIS_AUTHOR);
MODULE_DESCRIPTION(THIS_DESCRIPTION);