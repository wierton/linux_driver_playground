#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/uio.h>
#include <linux/vmalloc.h>

#define GBLFIFO_MAJOR 230
#define GBLFIFO_SIZE 1024

#define klog(fmt, ...) \
  printk(KERN_INFO "%s:%d: " fmt, __func__, __LINE__, ## __VA_ARGS__)

struct gblfifo_dev {
  unsigned curl;
  struct cdev cdev;
  struct mutex mutex;
  wait_queue_head_t r_wait;
  wait_queue_head_t w_wait;
  uint8_t mem[GBLFIFO_SIZE];
};

static struct gblfifo_dev *gblfifo_devp = NULL;

static int gblfifo_open(struct inode *inode, struct file *filp) {
  filp->private_data = container_of(inode->i_cdev, struct gblfifo_dev, cdev);
  return 0;
}

static ssize_t gblfifo_read(
    struct file *filp, char __user *buf, size_t len, loff_t *ppos) {
  int ret = 0;
  struct gblfifo_dev *devp = filp->private_data;
  DECLARE_WAITQUEUE(wait, current);

  mutex_lock(&devp->mutex);

  add_wait_queue(&devp->r_wait, &wait);

  while (devp->curl == 0) {
    if (filp->f_flags & O_NONBLOCK) {
      ret = -EAGAIN;
      goto out;
    }

    mutex_unlock(&devp->mutex);
    set_current_state(TASK_INTERRUPTIBLE);
    schedule();

    if (signal_pending(current)) {
      remove_wait_queue(&devp->r_wait, &wait);
      set_current_state(TASK_RUNNING);
      return -ERESTARTSYS;
    }
    mutex_lock(&devp->mutex);
  }

  if (len > devp->curl) len = devp->curl;

  if (copy_to_user(buf, devp->mem, len)) {
    ret = -EFAULT;
    goto out;
  } else {
    memcpy(devp->mem, devp->mem + len, devp->curl - len);
    devp->curl -= len;
    wake_up_interruptible(&devp->w_wait);
    ret = len;
  }

out:
  remove_wait_queue(&devp->r_wait, &wait);
  mutex_unlock(&devp->mutex);
  set_current_state(TASK_RUNNING);
  return ret;
}

static ssize_t gblfifo_write(
    struct file *filp, const char __user *buf, size_t len, loff_t *ppos) {
  int ret = 0;
  struct gblfifo_dev *devp = filp->private_data;

  DECLARE_WAITQUEUE(wait, current);

  mutex_lock(&devp->mutex);
  add_wait_queue(&devp->w_wait, &wait);

  while (devp->curl >= GBLFIFO_SIZE) {
    if (filp->f_flags & O_NONBLOCK) {
      ret = -EAGAIN;
      goto out;
    }

    mutex_unlock(&devp->mutex);
    set_current_state(TASK_INTERRUPTIBLE);
    schedule();

    if (signal_pending(current)) {
      remove_wait_queue(&devp->w_wait, &wait);
      set_current_state(TASK_RUNNING);
      return -ERESTARTSYS;
    }
    mutex_lock(&devp->mutex);
  }

  if (len >= GBLFIFO_SIZE - devp->curl) len = GBLFIFO_SIZE - devp->curl;

  if (copy_from_user(devp->mem + devp->curl, buf, len)) {
    ret = -EFAULT;
  } else {
    devp->curl += len;
    wake_up_interruptible(&devp->r_wait);
    ret = len;
  }

out:
  remove_wait_queue(&devp->w_wait, &wait);
  mutex_unlock(&devp->mutex);
  set_current_state(TASK_RUNNING);
  return ret;
}

static loff_t gblfifo_llseek(struct file *filp, loff_t offset, int orig) {
  return -EINVAL;
}

static long gblfifo_ioctl(
    struct file *filp, unsigned int cmd, unsigned long arg) {
  int err_code = 0;
  unsigned long size = GBLFIFO_SIZE;
  switch (cmd) {
  case BLKGETSIZE:
    err_code = copy_to_user((char __user *)arg, &size, sizeof(arg));
    if (err_code < 0) return -EFAULT;
    return 0;
  case BLKGETSIZE64:
    err_code = copy_to_user((char __user *)arg, &size, sizeof(arg));
    if (err_code < 0) return -EFAULT;
    return 0;
  default: return -EINVAL;
  }
}

static const struct file_operations gblfifo_ops = {
    .owner = THIS_MODULE,
    .open = gblfifo_open,
    .read = gblfifo_read,
    .write = gblfifo_write,
    .llseek = gblfifo_llseek,
    .unlocked_ioctl = gblfifo_ioctl,
#if 0
  .ioctl = xx,
#endif
};

static int __init gblfifo_init(void) {
  int err_code = 0;
  dev_t dev = MKDEV(GBLFIFO_MAJOR, 0);
  gblfifo_devp = vzalloc(sizeof(struct gblfifo_dev));

  klog("gblfifo_init %s:%d\n", __func__, __LINE__);
  if (!gblfifo_devp) goto error_malloc;

  klog("gblfifo_init %s:%d\n", __func__, __LINE__);
  err_code = register_chrdev_region(dev, 1, "gblfifo");
  if (err_code < 0) goto error_register_region;

  klog("gblfifo_init %s:%d\n", __func__, __LINE__);
  cdev_init(&gblfifo_devp->cdev, &gblfifo_ops);
  klog("gblfifo_init %s:%d\n", __func__, __LINE__);
  gblfifo_devp->cdev.owner = THIS_MODULE;
  klog("gblfifo_init %s:%d\n", __func__, __LINE__);
  err_code = cdev_add(&gblfifo_devp->cdev, dev, 1);
  klog("gblfifo_init %s:%d\n", __func__, __LINE__);
  if (err_code < 0) goto error_cdev_add;

  klog("gblfifo_init %s:%d\n", __func__, __LINE__);

  mutex_init(&gblfifo_devp->mutex);
  init_waitqueue_head(&gblfifo_devp->r_wait);
  init_waitqueue_head(&gblfifo_devp->w_wait);
  klog("gblfifo_init success\n");

  return 0;

error_cdev_add:
  klog("Fail to invoke cdev_add\n");

error_register_region:
  vfree(gblfifo_devp);
  return err_code;

error_malloc:
  return -ENOMEM;
}

static void __exit gblfifo_exit(void) {
  if (gblfifo_devp) {
    cdev_del(&gblfifo_devp->cdev);
    vfree(gblfifo_devp);
  }
  unregister_chrdev_region(MKDEV(GBLFIFO_MAJOR, 0), 1);
}

module_init(gblfifo_init);
module_exit(gblfifo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wierton");
MODULE_DESCRIPTION("A simple global memory");
