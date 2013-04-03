/*
 *
 *  Minos - Kernel Circular Buffer for logging inside /proc
 *  
 *  Dragos-Adrian Seredinschi, November, 2012
 *  
 */

/*
 * __KERNEL__: Code usable only in the kernel
 */
#undef __KERNEL__
#define __KERNEL__

/*
 * Not sure what this does / why it's needed
 */
#undef MODULE
#define MODULE

/* 
 * Linux Kernel/LKM headers: module.h is needed by all modules and kernel.h is needed for KERN_INFO.
 */
#include <linux/module.h>   // included for all kernel modules
#include <linux/kernel.h>   // included for KERN_INFO
#include <linux/init.h>     // included for __init and __exit macros
#include <linux/slab.h>     // for kmalloc / kfree
#include <linux/errno.h>
#include <linux/proc_fs.h> 

#include <linux/string.h>


/* 
 * Module basic definitions
 */
#define THIS_AUTHOR "Dragos-Adrian Seredinschi <adizere@cpan.org>"
#define THIS_NAME "minos"
#define THIS_DESCRIPTION "Kernel circular buffer for logging inside /proc"

#define _ERROR_EXIT -1
#define _SUCCESS_EXIT 0

#define _LOG_SIZE 1600


/* Macros fun
 */ 
#define FORMAT_LOG_ENTRY(entry) "[" THIS_NAME "] " entry "\n"


/* The log will essentially be composed from multiple entries, each of them
 * having the following structure
 */
struct log_entry
{
    char* data;
    size_t length;
    struct log_entry* next;
};


/* We'll physically log in a proc file */
static struct proc_dir_entry* proc_entry;

/* The ring buffer, each buffer entry will be a log entry */
static struct log_entry* log_entry_head;

/* Sum of the entries kept in the ring buffer */
static unsigned int data_length_sum = 0;

/*
TODO:
 - proper KERN_* printk() log levels
*/


/*
 ** Internal Method
 * _consume_log_entries
 * 
 * Traverses all the log entries starting from log_entry_head and copies them
 * into the provided arguments buf.
 * If clear_on_consume is set to true then all entries will be freed.
 */
int _consume_log_entries(bool clear_on_consume, char* buf)
{
    int len = 0;
    struct log_entry* current_entry;

    /* Now consume */
    current_entry = log_entry_head;
    while(current_entry != NULL)
    {
        struct log_entry* next_entry;

        /* Only save the entries if a buf was provided */
        if (buf != NULL)
        {
            len += sprintf(buf+len, "%s\n", current_entry->data);
        }
        
        next_entry = current_entry->next;

        /* Clear entries if requested to do so */
        if (clear_on_consume == true)
        {
            kfree(current_entry->data);
            kfree(current_entry);
            current_entry->data = NULL;
            current_entry = NULL;
        }

        /* Advance to the next entry */
        current_entry = next_entry;
    }

    /* Reset the data length sum */
    if (clear_on_consume == true)
    {
        log_entry_head = NULL;
        data_length_sum = 0;        
    }

    return len;
}


/*
 ** Internal Method
 * _log_entry_from_data
 * 
 * Creates a new log_entry structure properly initialized from the provided
 * string in the parameter in_data. 
 */ 
struct log_entry* _log_entry_from_data(char* in_data)
{
    size_t data_length = 0;
    struct log_entry* new_entry = NULL;
    
    data_length = strlen(in_data) + 1;

    new_entry = (struct log_entry*)
        kmalloc(sizeof(struct log_entry), GFP_KERNEL);
    if (new_entry == NULL)
    {
        printk(KERN_INFO FORMAT_LOG_ENTRY("Error creating new log_entry from "
            "data: failed at memory allocation."));
        return NULL;
    }

    new_entry->length = data_length;
    new_entry->next = NULL;
    new_entry->data = (char*)kmalloc(data_length, GFP_KERNEL);
    strncpy(new_entry->data, in_data, data_length);
    if (new_entry->data == NULL)
    {
        printk(KERN_INFO FORMAT_LOG_ENTRY("Error creating new log_entry->data "
            "from data: failed at memory allocation."));
        return NULL;
    }

    new_entry->data[data_length-1] = '\0';

    return new_entry;
}


/*
 ** Internal Method
 * _insert_log_entry
 * 
 * Adds a new entry to the circular buffer pointed by log_entry_head.
 * This method assures that the total data in the circular buffer will not
 * exceed _LOG_SIZE.
 */ 
bool _insert_log_entry(struct log_entry* new_entry)
{
    struct log_entry* temp_entry = NULL; 

    printk(KERN_INFO "Inserting new log entry.\n");

    /* The new entry is too big! */
    if (new_entry->length > _LOG_SIZE)
    {
        printk(KERN_INFO FORMAT_LOG_ENTRY("New entry is too big."));
        return false;
    }

    /* Clear the oldest entry to make space for the new one */
    while (data_length_sum + new_entry->length > _LOG_SIZE)
    {
        printk(KERN_INFO "Sum %d bigger than %d.\n", data_length_sum + new_entry->length, _LOG_SIZE);

        temp_entry = log_entry_head->next;

        data_length_sum -= log_entry_head->length;

        kfree(log_entry_head->data);
        kfree(log_entry_head);

        log_entry_head = temp_entry;
    }

    data_length_sum += new_entry->length;

    /* Buffer empty; new entry will be the head */
    if (log_entry_head == NULL)
    {
        printk(KERN_INFO "Head is null.");
        log_entry_head = new_entry;
        return true;
    }

    /* Buffer not empty; traverse and put the entry in the tail */
    temp_entry = log_entry_head;
    while(temp_entry->next != NULL)
    {
        printk(KERN_INFO "Traversing to reach the end of the list.\n");
        temp_entry = temp_entry->next; 
    }
    temp_entry->next = new_entry;
    return true;
}


/*
 ** Proc Entry Method for Read
 * fetch_log_data
 */ 
int fetch_log_data(char *buf, char **start, off_t offset, int count, int *eof, 
    void *data)
{
    int len = 0;

    printk(KERN_INFO "Sum: %d.\n", data_length_sum);

    /* Consume the entries and put the data in buf */
    len = _consume_log_entries(false, buf);

    printk(KERN_INFO "----- Read served.\n");

    return len;
}


/*
 ** Proc Entry Method for Write
 * clear_log_data
 */ 
int clear_log_data(struct file *file, const char __user *buffer,
                           unsigned long count, void *data)
{
    /* Consume the data without conserving it */
    _consume_log_entries(true, NULL);

    return 1;
}


/*
 ** EXPORTED symbol that will be called from external modules
 * add_log_data
 *
 * A new log_entry is created from the provided data and inserted in the
 * circular buffer.
 */
static void add_log_data(char* data)
{
    struct log_entry* new_entry = NULL;
    bool add_status;

    new_entry = _log_entry_from_data(data);
    add_status = _insert_log_entry(new_entry);

    if (add_status == true)
    {
        printk(KERN_INFO FORMAT_LOG_ENTRY("New entry added!"));
    }
    else {
        printk(KERN_INFO FORMAT_LOG_ENTRY("Failed to add new entry!"));
    }
}


static int __init minos_init(void)
{
    printk(KERN_INFO FORMAT_LOG_ENTRY("Loading module."));

    proc_entry = create_proc_entry(THIS_NAME, 0666, NULL);
    if(proc_entry == NULL)
    {
        printk(KERN_INFO "Error creating proc entry");
        return _ERROR_EXIT;
    }

    proc_entry->size = _LOG_SIZE;
    proc_entry->read_proc = fetch_log_data;
    proc_entry->write_proc = clear_log_data;

    printk(KERN_INFO FORMAT_LOG_ENTRY("All good."));

    return _SUCCESS_EXIT;
}


static void __exit minos_cleanup(void)
{
    _consume_log_entries(true, NULL);

    remove_proc_entry(THIS_NAME, NULL);
    
    printk(KERN_INFO FORMAT_LOG_ENTRY("Cleaning up module."));
}


EXPORT_SYMBOL(add_log_data);

module_init(minos_init);
module_exit(minos_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(THIS_AUTHOR);
MODULE_DESCRIPTION(THIS_DESCRIPTION);

/*
 * END
 */