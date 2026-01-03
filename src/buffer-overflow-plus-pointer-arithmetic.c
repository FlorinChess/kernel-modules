#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/pci.h>

#define DEVICE_BASENAME "userdata"
#define KERNEL_BUF_SIZE 128    /* kernel-local buffer filled with pattern */
#define USER_BUF_MAX    256    /* maximum user buffer size to accept */
#define RESULT_BUF_SIZE 16     /* small area to hold results to return */

static dev_t userdata_devnum;
static struct cdev userdata_cdev;
static DEFINE_MUTEX(userdata_lock);

static char kernel_pattern[KERNEL_BUF_SIZE];
static char user_copy[USER_BUF_MAX];
static size_t user_copy_len;
static char computed_result = 0;

static ssize_t userdata_dev_write(struct file *file, const char __user *buf,
                                  size_t count, loff_t *ppos)
{
    int tainted_offset;
    size_t bounded;
    ssize_t ret = count;

    if (!mutex_trylock(&userdata_lock))
        return -EBUSY;

    /* Case A: write an int (tainted scalar offset) */
    if (count == sizeof(int)) {
        if (copy_from_user(&tainted_offset, buf, sizeof(int))) {
            pr_err("userdata: copy_from_user (offset) failed\n");
            ret = -EFAULT;
            goto out;
        }

        pr_info("userdata: received tainted offset=%d via copy_from_user\n",
                tainted_offset);

        bounded = (size_t) (tainted_offset % KERNEL_BUF_SIZE);
        computed_result = *(kernel_pattern + bounded);

        pr_info("userdata: selected kernel_pattern[%zu] = '%c'\n",
                bounded, computed_result);

        /* If we have a user buffer stored, also use tainted_offset to index it */
        if (user_copy_len > 0) {
            /* further bound to avoid OOB in user_copy */
            bounded = (size_t) (tainted_offset % user_copy_len);
            /* pointer arithmetic on kernel-copied user buffer (tainted data source) */
            computed_result ^= *(user_copy + bounded); /* combine results to create observable dependence */
            pr_info("userdata: combined with user_copy[%zu] => computed_result='%c' (xor view)\n",
                    bounded, computed_result);
        }

        goto out;
    }

    /* Case B: write a buffer (tainted buffer content) */
    if (count > 0) {
        size_t to_copy = (count > USER_BUF_MAX) ? USER_BUF_MAX : count;

        if (copy_from_user(user_copy, buf, to_copy)) {
            pr_err("userdata: copy_from_user (buffer) failed\n");
            ret = -EFAULT;
            goto out;
        }
        user_copy_len = to_copy;

        pr_info("userdata: copied %zu bytes from userspace into user_copy (tainted buffer)\n",
                user_copy_len);

        /* Example pointer arithmetic using the user-supplied buffer contents:
         * - take the first byte of user_copy as a (small) tainted scalar value
         * - use that as an offset into kernel_pattern
         */
        if (user_copy_len >= 1) {
            unsigned char first = (unsigned char)user_copy[0];

            /* Bound 'first' to avoid out-of-range read; keep data dependency */
            bounded = (size_t)(first % KERNEL_BUF_SIZE);

            computed_result = *(kernel_pattern + bounded);
            pr_info("userdata: pointer arithmetic using user_copy[0]=%u -> kernel_pattern[%zu] = '%c'\n",
                    (unsigned)first, bounded, computed_result);
        }
        goto out;
    }

    /* If we get here, nothing done */
    pr_warn("userdata: write: nothing done (count=%zu)\n", count);

out:
    mutex_unlock(&userdata_lock);
    return ret;
}

static ssize_t userdata_dev_read(struct file *file, char __user *buf,
                                 size_t count, loff_t *ppos)
{
    ssize_t ret = 0;

    if (!mutex_trylock(&userdata_lock))
        return -EBUSY;

    if (count < 1) {
        ret = -EINVAL;
        goto out;
    }

    /* Sink #1: copy_to_user */
    if (copy_to_user(buf, &computed_result, 1)) {
        pr_err("userdata: copy_to_user failed\n");
        ret = -EFAULT;
        goto out;
    }

    /* Also show put_user (alternate sink) if user buffer is aligned for an int write */
    /* For demonstration only; ignore its return if not used. */
    /* put_user(computed_result, (char __user *)buf); */

    pr_info("userdata: returned computed_result '%c' to userspace via copy_to_user\n",
            computed_result);

    ret = 1;

out:
    mutex_unlock(&userdata_lock);
    return ret;
}

static int userdata_dev_open(struct inode *inode, struct file *file)
{
    /* allow single-open semantics with the mutex in write/read */
    pr_info("userdata: device opened\n");
    return 0;
}

static int userdata_dev_release(struct inode *inode, struct file *file)
{
    pr_info("userdata: device closed\n");
    return 0;
}

static const struct file_operations userdata_fops = {
    .owner   = THIS_MODULE,
    .read    = userdata_dev_read,
    .write   = userdata_dev_write,
    .open    = userdata_dev_open,
    .release = userdata_dev_release,
};

static int __init userdata_init(void)
{
    int err, i;
    char dev_name[64];

    pr_info("userdata: initializing module\n");

    /* register character device */
    err = alloc_chrdev_region(&userdata_devnum, 0, 1, DEVICE_BASENAME);
    if (err) {
        pr_err("userdata: alloc_chrdev_region failed: %d\n", err);
        return err;
    }

    cdev_init(&userdata_cdev, &userdata_fops);
    userdata_cdev.owner = THIS_MODULE;

    err = cdev_add(&userdata_cdev, userdata_devnum, 1);
    if (err) {
        pr_err("userdata: cdev_add failed: %d\n", err);
        unregister_chrdev_region(userdata_devnum, 1);
        return err;
    }

    /* init kernel pattern */
    for (i = 0; i < KERNEL_BUF_SIZE; i++)
        kernel_pattern[i] = (i % 2) ? 'K' : 'k';

    user_copy_len = 0;
    computed_result = kernel_pattern[0];

    snprintf(dev_name, sizeof(dev_name),
             "/dev/%s (major %d, minor %d)",
             DEVICE_BASENAME, MAJOR(userdata_devnum), MINOR(userdata_devnum));

    pr_info("userdata: registered device %s\n", dev_name);
    pr_info("userdata: create node: sudo mknod /dev/%s c %d %d && sudo chown $USER /dev/%s\n",
            DEVICE_BASENAME, MAJOR(userdata_devnum), MINOR(userdata_devnum), DEVICE_BASENAME);

    mutex_init(&userdata_lock);

    return 0;
}

static void __exit userdata_exit(void)
{
    pr_info("userdata: exiting module\n");

    cdev_del(&userdata_cdev);
    unregister_chrdev_region(userdata_devnum, 1);

    /* clear sensitive data */
    if (user_copy_len > 0)
        memset(user_copy, 0, user_copy_len);
    user_copy_len = 0;
}

module_init(userdata_init);
module_exit(userdata_exit);

MODULE_AUTHOR("Florin Zamfir");
MODULE_DESCRIPTION("Buffer overflow and faulty pointer arithmetic vulenrability demo.");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
