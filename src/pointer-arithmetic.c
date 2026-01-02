#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>        /* alloc_chrdev_region, struct file_operations */
#include <linux/cdev.h>
#include <linux/uaccess.h>   /* copy_from_user, copy_to_user, get_user, put_user */
#include <linux/slab.h>      /* kmalloc, kfree */

#define DEVICE_NAME "taintdev"
#define KBUF_SIZE 64

static dev_t dev;
static struct cdev cdev;
static char *kbuf;             
static char last_result = 0;   
static DEFINE_MUTEX(dev_lock);

static ssize_t taint_write(struct file *file, const char __user *ubuf,
                           size_t count, loff_t *ppos)
{
    int tainted_offset;
    size_t bounded;
    char selected;

    if (!mutex_trylock(&dev_lock))
        return -EBUSY;

    if (count < sizeof(int)) {
        pr_info("taintdev: write: need at least %zu bytes (got %zu)\n",
                sizeof(int), count);
        mutex_unlock(&dev_lock);
        return -EINVAL;
    }

    // taint source
    if (copy_from_user(&tainted_offset, ubuf, sizeof(tainted_offset))) {
        pr_err("taintdev: copy_from_user failed\n");
        mutex_unlock(&dev_lock);
        return -EFAULT;
    }

    pr_info("taintdev: write: got tainted_offset=%d (raw)\n", tainted_offset);

    // TODO: make sure to prevent kernel memory access
    // if (tainted_offset < 0)
    //     tainted_offset = 0;

    bounded = (size_t)(tainted_offset % KBUF_SIZE);

    // !!
    selected = *(kbuf + bounded);

    last_result = selected;

    pr_info("taintdev: pointer arithmetic: selected kbuf[%zu] = %c\n",
            bounded, selected);

    mutex_unlock(&dev_lock);

    return count;
}

static ssize_t taint_read(struct file *file, char __user *ubuf,
                          size_t count, loff_t *ppos)
{
    ssize_t ret;

    if (!mutex_trylock(&dev_lock))
        return -EBUSY;

    if (count < 1) {
        mutex_unlock(&dev_lock);
        return -EINVAL;
    }

    // taint sink
    if (copy_to_user(ubuf, &last_result, 1)) {
        pr_err("taintdev: copy_to_user failed\n");
        ret = -EFAULT;
    } else {
        /* Also log via pr_info (another sink — may be considered an info leak) */
        pr_info("taintdev: read: returned '%c' to userspace via copy_to_user\n",
                last_result);
        ret = 1;
    }

    mutex_unlock(&dev_lock);
    return ret;
}

static int taint_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&dev_lock))
        return -EBUSY;
    mutex_unlock(&dev_lock);
    return 0;
}

static int taint_release(struct inode *inode, struct file *file)
{
    return 0;
}

static const struct file_operations taint_fops = {
    .owner   = THIS_MODULE,
    .read    = taint_read,
    .write   = taint_write,
    .open    = taint_open,
    .release = taint_release,
};

static int __init taint_init(void)
{
    int err, i;
    char name[20];

    pr_info("taintdev: init\n");

    err = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
    if (err) {
        pr_err("taintdev: alloc_chrdev_region failed: %d\n", err);
        return err;
    }

    cdev_init(&cdev, &taint_fops);
    cdev.owner = THIS_MODULE;

    err = cdev_add(&cdev, dev, 1);
    if (err) {
        pr_err("taintdev: cdev_add failed: %d\n", err);
        unregister_chrdev_region(dev, 1);
        return err;
    }

    kbuf = kmalloc(KBUF_SIZE, GFP_KERNEL);
    if (!kbuf) {
        pr_err("taintdev: kmalloc failed\n");
        cdev_del(&cdev);
        unregister_chrdev_region(dev, 1);
        return -ENOMEM;
    }
    for (i = 0; i < KBUF_SIZE; i++)
        kbuf[i] = 'A' + (i % 26);

    last_result = kbuf[0];

    snprintf(name, sizeof(name), "%s (major %d, minor %d)",
             DEVICE_NAME, MAJOR(dev), MINOR(dev));
    pr_info("taintdev: registered %s\n", name);

    pr_info("taintdev: create device node: sudo mknod /dev/%s c %d %d\n",
            DEVICE_NAME, MAJOR(dev), MINOR(dev));

    return 0;
}

static void __exit taint_exit(void)
{
    pr_info("taintdev: exit\n");

    if (kbuf)
        kfree(kbuf);

    cdev_del(&cdev);
    unregister_chrdev_region(dev, 1);
}

module_init(taint_init);
module_exit(taint_exit);

MODULE_AUTHOR("Florin Zamfir");
MODULE_DESCRIPTION("Faulty pointer arithmetic vulenrability demo.");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
