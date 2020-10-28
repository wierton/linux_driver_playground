#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/module.h>
#include <linux/uio.h>

#define GBLMEM_MAJOR 230
#define GBLMEM_SIZE 1024

struct gblmem_dev {
  struct cdev cdev;
  uint8_t mem[GBLMEM_SIZE];
};

static struct gblmem_dev *gblmem_devp = NULL;

static int gblmem_open(
    struct inode *inode, struct file *filp) {
  filp->private_data =
      container_of(inode->i_cdev, struct gblmem_dev, cdev);
  return 0;
}

static ssize_t gblmem_read(struct file *filp,
    char __user *buf, size_t len, loff_t *ppos) {
  struct gblmem_dev *devp = container_of(
      filp->private_data, struct gblmem_dev, mem);
  loff_t pos = *ppos;
  if (pos < 0 || pos >= GBLMEM_SIZE) return -EINVAL;

  if (pos + len >= GBLMEM_SIZE) len = GBLMEM_SIZE - pos;

  if (copy_to_user(buf, devp->mem + pos, len)) {
    return -EFAULT;
  } else {
    *ppos += len;
    return len;
  }
}

static ssize_t gblmem_write(struct file *filp,
    const char __user *buf, size_t len, loff_t *ppos) {
  struct gblmem_dev *devp = container_of(
      filp->private_data, struct gblmem_dev, mem);
  loff_t pos = *ppos;
  if (pos < 0 || pos >= GBLMEM_SIZE) return -EINVAL;

  if (pos + len >= GBLMEM_SIZE) len = GBLMEM_SIZE - pos;

  if (copy_from_user(devp->mem + pos, buf, len)) {
    return -EFAULT;
  } else {
    *ppos += len;
    return len;
  }
}

static loff_t gblmem_llseek(
    struct file *filp, loff_t offset, int orig) {
  switch (orig) {
  case SEEK_SET:
    if (offset < 0) return -EINVAL;
    if (offset > GBLMEM_SIZE) return -EINVAL;
    filp->f_pos = (unsigned int)offset;
    return offset;
  case SEEK_CUR:
    if ((filp->f_pos + offset) > GBLMEM_SIZE)
      return -EINVAL;
    if ((filp->f_pos + offset) < 0)
      return -EINVAL;
    filp->f_pos += offset;
    break;
  case SEEK_END:
    filp->f_pos = GBLMEM_SIZE;
    break;
  default:
    return -EINVAL;
  }
  return filp->f_pos;
}

static const struct file_operations gblmem_ops = {
    .owner = THIS_MODULE,
    .open = gblmem_open,
    .read = gblmem_read,
    .write = gblmem_write,
    .llseek = gblmem_llseek,
#if 0
  .ioctl = xx,
#endif
};

static int __init gblmem_init(void) {
  int ret = 0;
  dev_t dev = MKDEV(GBLMEM_MAJOR, 0);
  gblmem_devp = vzalloc(sizeof(struct gblmem_dev));

  if (!gblmem_devp) goto error_malloc;

  ret = register_chrdev_region(dev, 1, "gblmem");
  if (ret) goto error;

  cdev_init(&gblmem_devp->cdev, &gblmem_ops);
  gblmem_devp->cdev.owner = THIS_MODULE;
  ret = cdev_add(&gblmem_devp->cdev, dev, 1);
  if (ret) goto error_region;

  return 0;

error_region:
  unregister_chrdev_region(dev, 1);
error:
  return ret;

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
