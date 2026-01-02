#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/slab.h> 
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
MODULE_DESCRIPTION("Buffer overflow vulenrability demo.");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
