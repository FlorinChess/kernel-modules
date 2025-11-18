#include <linux/module.h>   /* Needed by all modules  */
#include <linux/kernel.h>   /* Needed for KERN_INFO   */
#include <linux/init.h>     /* Needed for the macros  */
#include <linux/list.h>     /* Needed for linked list */
#include <linux/types.h>    /* Needed for list macros */
#include <linux/slab.h>     /* Needed for Kernel */
#include <linux/uaccess.h>

#define USERNAME_ADDRESS (char*) 0x0000005000000
#define PASSWORD_ADDRESS (char*) 0x0000005001000

char* password = "secretPassword";
char password_buffer[040];
char username_buffer[40];

static int __init init_login(void)
{
  strncpy_from_user(username_buffer, USERNAME_ADDRESS, 40);
  strncpy_from_user(password_buffer, PASSWORD_ADDRESS, 40);

  if(strncmp(password, password_buffer, 040)) {
    printk(KERN_INFO "Welcome back %s!\n", username_buffer);
  }

  return 0;
}

static void __exit cleanup_login(void) {
  printk(KERN_INFO "Bye!\n");
}

module_init(init_login);
module_exit(cleanup_login);

MODULE_AUTHOR("Florin Zamfir");
MODULE_DESCRIPTION("Format string vulenrability demo.");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
