#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/uio.h>
#include <linux/vmalloc.h>

#define GBLMEM_MAJOR 230
#define GBLMEM_SIZE 1024

struct gblmem_dev {
  struct cdev cdev;
  struct mutex mutex;
  uint8_t mem[GBLMEM_SIZE];
};

static struct gblmem_dev *gblmem_devp = NULL;

static int gblmem_open(struct inode *inode, struct file *filp) {
  filp->private_data = container_of(inode->i_cdev, struct gblmem_dev, cdev);
  return 0;
}

static ssize_t gblmem_read(
    struct file *filp, char __user *buf, size_t len, loff_t *ppos) {
  int ret = 0;
  struct gblmem_dev *devp = filp->private_data;
  loff_t pos = *ppos;
  if (pos < 0) return -EINVAL;
  if (pos >= GBLMEM_SIZE) return 0;

  /* avoid ops + len overflow */
  if (len >= GBLMEM_SIZE - pos) len = GBLMEM_SIZE - pos;

  mutex_lock(&devp->mutex);
  ret = copy_to_user(buf, devp->mem + pos, len);
  if (ret >= 0) {
    *ppos += len;
    ret = len;
  }
  mutex_unlock(&devp->mutex);

  return ret;
}

static ssize_t gblmem_write(
    struct file *filp, const char __user *buf, size_t len, loff_t *ppos) {
  int ret = 0;
  struct gblmem_dev *devp = filp->private_data;
  loff_t pos = *ppos;
  if (pos < 0) return -EINVAL;
  if (pos >= GBLMEM_SIZE) return 0;

  if (len >= GBLMEM_SIZE - pos) len = GBLMEM_SIZE - pos;

  mutex_lock(&devp->mutex);
  ret = copy_from_user(devp->mem + pos, buf, len);
  if (ret >= 0) {
    *ppos += len;
    ret = len;
  }
  mutex_unlock(&devp->mutex);

  return ret;
}

static loff_t gblmem_llseek(struct file *filp, loff_t offset, int orig) {
  loff_t ret = 0;
  struct gblmem_dev *devp = filp->private_data;

  mutex_lock(&devp->mutex);
  switch (orig) {
  case SEEK_SET:
    if (offset < 0)
      ret = -EINVAL;
    else if (offset > GBLMEM_SIZE)
      ret = -EINVAL;
    else {
      filp->f_pos = (unsigned int)offset;
    }
    break;
  case SEEK_CUR:
    if ((filp->f_pos + offset) > GBLMEM_SIZE)
      ret = -EINVAL;
    else if ((filp->f_pos + offset) < 0)
      ret = -EINVAL;
    else
      filp->f_pos += offset;
    break;
  case SEEK_END: filp->f_pos = GBLMEM_SIZE; break;
  default: ret = -EINVAL; break;
  }
  ret = filp->f_pos;
  mutex_unlock(&devp->mutex);

  return ret;
}

static long gblmem_ioctl(
    struct file *filp, unsigned int cmd, unsigned long arg) {
  int err_code = 0;
  unsigned long size = GBLMEM_SIZE;
  switch (cmd) {
  case BLKGETSIZE:
    err_code = copy_to_user((char __user *)arg, &size, sizeof(arg));
    printk("copy_to_user: %p, %lu, %d", (char *)arg, size, err_code);
    if (err_code < 0) return -EFAULT;
    return 0;
  case BLKGETSIZE64:
    err_code = copy_to_user((char __user *)arg, &size, sizeof(arg));
    printk("copy_to_user: %p, %lu, %d", (char *)arg, size, err_code);
    if (err_code < 0) return -EFAULT;
    return 0;
  default: return -EINVAL;
  }
}

static const struct file_operations gblmem_ops = {
    .owner = THIS_MODULE,
    .open = gblmem_open,
    .read = gblmem_read,
    .write = gblmem_write,
    .llseek = gblmem_llseek,
    .unlocked_ioctl = gblmem_ioctl,
#if 0
  .ioctl = xx,
#endif
};

static int __init gblmem_init(void) {
  int err_code = 0;
  dev_t dev = MKDEV(GBLMEM_MAJOR, 0);
  gblmem_devp = vzalloc(sizeof(struct gblmem_dev));

  if (!gblmem_devp) goto error_malloc;

  err_code = register_chrdev_region(dev, 1, "gblmem");
  if (err_code < 0) goto error_register_region;

  cdev_init(&gblmem_devp->cdev, &gblmem_ops);
  gblmem_devp->cdev.owner = THIS_MODULE;
  err_code = cdev_add(&gblmem_devp->cdev, dev, 1);
  if (err_code < 0) goto error_cdev_add;

  mutex_init(&gblmem_devp->mutex);

  return 0;

error_cdev_add:
  printk("Fail to invoke cdev_add\n");

error_register_region:
  vfree(gblmem_devp);
  return err_code;

error_malloc:
  return -ENOMEM;
}

static void __exit gblmem_exit(void) {
  if (gblmem_devp) {
    cdev_del(&gblmem_devp->cdev);
    vfree(gblmem_devp);
  }
  unregister_chrdev_region(MKDEV(GBLMEM_MAJOR, 0), 1);
}

module_init(gblmem_init);
module_exit(gblmem_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wierton");
MODULE_DESCRIPTION("A simple global memory");
