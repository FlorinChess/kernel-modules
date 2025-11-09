#include <linux/module.h>   /* Needed by all modules  */
#include <linux/kernel.h>   /* Needed for KERN_INFO   */
#include <linux/init.h>     /* Needed for the macros  */
#include <linux/list.h>     /* Needed for linked list */
#include <linux/types.h>    /* Needed for list macros */
#include <linux/slab.h>     /* Needed for Kernel */
#include <linux/uaccess.h>

#define READ_SIZE 16
#define USERNAME_ADDRESS (char*)0x0000005004000

void print_username(char* username) {
  printk(username);

  username = NULL;
}

static int __init init_username_reader(void)
{
  char username_buffer[READ_SIZE];
  strncpy_from_user(username_buffer, USERNAME_ADDRESS, READ_SIZE);

  print_username(username_buffer);

  return 0;
}

static void __exit cleanup_username_reader(void) {
  printk(KERN_INFO "Bye!\n");
}

module_init(init_username_reader);
module_exit(cleanup_username_reader);

MODULE_AUTHOR("Florin Zamfir");
MODULE_DESCRIPTION("Format string vulenrability demo.");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
