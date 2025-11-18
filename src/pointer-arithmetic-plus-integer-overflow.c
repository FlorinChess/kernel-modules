// taint_kernel2.c
// Second taint-tracking demo module.
// Uses get_user()↦pointer arithmetic↦put_user() flow.
// License: GPL

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/slab.h>

#define DEVICE_NAME "taintdev2"
#define KBUF_SIZE   32

static dev_t dev;
static struct cdev cdev2;

static char *kbuf;
static char last_byte;

static ssize_t t2_write(struct file *f, const char __user *ubuf,
                        size_t count, loff_t *pos)
{
    int offset;
    size_t bounded;

    if (count < sizeof(int))
        return -EINVAL;

    if (get_user(offset, (int __user *)ubuf))
        return -EFAULT;

    pr_info("taintdev2: got tainted offset=%d via get_user\n", offset);

    if (offset < 0)
        offset = 0;

    bounded = offset % KBUF_SIZE;

    /* pointer arithmetic */
    last_byte = *(kbuf + bounded);

    pr_info("taintdev2: selected byte '%c' from kbuf[%zu]\n",
            last_byte, bounded);

    return count;
}

/*
 * Sink: put_user()
 */
static ssize_t t2_read(struct file *f, char __user *ubuf,
                       size_t count, loff_t *pos)
{
    if (count < 1)
        return -EINVAL;

    if (put_user(last_byte, ubuf))
        return -EFAULT;

    pr_info("taintdev2: returned '%c' via put_user\n", last_byte);

    return 1;
}

static const struct file_operations t2_fops = {
    .owner = THIS_MODULE,
    .write = t2_write,
    .read  = t2_read,
};

static int __init t2_init(void)
{
    int r, i;

    pr_info("taintdev2: loading\n");

    r = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
    if (r)
        return r;

    cdev_init(&cdev2, &t2_fops);
    cdev2.owner = THIS_MODULE;

    r = cdev_add(&cdev2, dev, 1);
    if (r) {
        unregister_chrdev_region(dev, 1);
        return r;
    }

    kbuf = kmalloc(KBUF_SIZE, GFP_KERNEL);
    if (!kbuf) {
        cdev_del(&cdev2);
        unregister_chrdev_region(dev, 1);
        return -ENOMEM;
    }

    for (i = 0; i < KBUF_SIZE; i++)
        kbuf[i] = 'a' + (i % 26);

    pr_info("taintdev2: ready (mknod /dev/%s c %d %d)\n",
            DEVICE_NAME, MAJOR(dev), MINOR(dev));

    return 0;
}

static void __exit t2_exit(void)
{
    pr_info("taintdev2: unloading\n");
    kfree(kbuf);
    cdev_del(&cdev2);
    unregister_chrdev_region(dev, 1);
}

module_init(t2_init);
module_exit(t2_exit);

MODULE_AUTHOR("Florin Zamfir");
MODULE_DESCRIPTION("Faulty pointer arithmetic vulenrability demo 2.");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
