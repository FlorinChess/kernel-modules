#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define DEVICE_NAME "memop_device"
#define BUF_SIZE 128

static int major;
static char* kernel_buffer;
static int write_count = 0;

static int dev_open(struct inode* inodep, struct file* filep);
static int dev_release(struct inode* inodep, struct file* filep);
static ssize_t dev_write(struct file* filep, const char __user* buffer, size_t len, loff_t* offset);

static struct file_operations fops = {
    .open = dev_open,
    .release = dev_release,
    .write = dev_write,
};

static int dev_open(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "[memop] Device opened.\n");
    return 0;
}

static int dev_release(struct inode* inodep, struct file* filep)
{
    printk(KERN_INFO "[memop] Device closed.\n");
    return 0;
}

static ssize_t dev_write(struct file* filep, const char __user* buffer, size_t len, loff_t* offset)
{
    long copied;
    size_t to_copy;

    // TODO figure out dynamic buffer increase
    // to_copy = (len > BUF_SIZE - 1) ? BUF_SIZE - 1 : len;
    to_copy = len;

    copied = strncpy_from_user(kernel_buffer, buffer, to_copy);
    if (copied < 0) {
        printk(KERN_ERR "[memop] Failed to copy data from user.\n");
        return -EFAULT;
    }

    kernel_buffer[to_copy] = '\0';  // Null-terminate string

    write_count++;
    printk(KERN_INFO "[memop] Received (%d): %s\n", write_count, kernel_buffer);

    for (size_t i = 0; i < to_copy; i++) {
        if (kernel_buffer[i] >= 'a' && kernel_buffer[i] <= 'z')
            kernel_buffer[i] -= 32;
        else if (kernel_buffer[i] >= 'A' && kernel_buffer[i] <= 'Z')
            kernel_buffer[i] += 32;
    }

    printk(KERN_INFO "[memop] Modified buffer: %s\n", kernel_buffer);

    return to_copy;
}

static int __init memop_init(void)
{
    printk(KERN_INFO "[memop] Initializing module.\n");

    kernel_buffer = kmalloc(BUF_SIZE, GFP_KERNEL);
    if (!kernel_buffer) {
        printk(KERN_ERR "[memop] Failed to allocate memory.\n");
        return -ENOMEM;
    }

    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) {
        printk(KERN_ERR "[memop] Failed to register char device.\n");
        kfree(kernel_buffer);
        return major;
    }

    printk(KERN_INFO "[memop] Module loaded. Major number: %d\n", major);
    return 0;
}

static void __exit memop_exit(void)
{
    unregister_chrdev(major, DEVICE_NAME);
    kfree(kernel_buffer);
    printk(KERN_INFO "[memop] Module unloaded.\n");
}

module_init(memop_init);
module_exit(memop_exit);

MODULE_AUTHOR("Florin Zamfir");
MODULE_DESCRIPTION("Buffer overflow + use-after-free vulnerability demo.");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
