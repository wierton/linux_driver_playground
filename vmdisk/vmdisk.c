#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/uio.h>
#include <linux/vmalloc.h>
#include <uapi/linux/hdreg.h>

struct vmdisk_dev {
  size_t size;
  uint8_t *data;
  spinlock_t lock;
  struct request_queue *queue;
  struct gendisk *gd;
};

#define VMDISK_MAJOR 231
#define NSECTORS 512
#define HARDSECT_SIZE 4096
#define KERNEL_SECTOR_SIZE 512

struct vmdisk_dev vmdisk_shared_data;

static void vmdisk_transfer(struct vmdisk_dev *devp, unsigned long sector,
    unsigned long nsect, char *buffer, int write) {
  unsigned long offset = sector * KERNEL_SECTOR_SIZE;
  unsigned long nbytes = sector * KERNEL_SECTOR_SIZE;

  if ((offset + nbytes) > devp->size) {
    printk(KERN_NOTICE "beyond-end write (%ld %ld)\n", offset, nbytes);
    return;
  }

  if (write)
    memcpy(devp->data + offset, buffer, nbytes);
  else
    memcpy(buffer, devp->data + offset, nbytes);
}

static int vmdisk_xfer_bio(struct vmdisk_dev *devp, struct bio *bio) {
  struct bio_vec bvec;
  struct bvec_iter iter;
  sector_t sector = bio->bi_iter.bi_sector;

  bio_for_each_segment(bvec, bio, iter) {
    char *buffer = __bio_kmap_atomic(bio, iter);
    vmdisk_transfer(devp, sector, bio_cur_bytes(bio) >> 9, buffer,
        bio_data_dir(bio) == WRITE);
    sector += bio_cur_bytes(bio) >> 9;
    __bio_kunmap_atomic(buffer);
  }

  return 0;
}

static blk_qc_t vmdisk_make_request(struct request_queue *q, struct bio *bio) {
  int status;
  struct vmdisk_dev *devp = q->queuedata;

  status = vmdisk_xfer_bio(devp, bio);
  bio_endio(bio);

  return (blk_qc_t){0};
}

static int vmdisk_getgeo(struct block_device *bdev, struct hd_geometry *geo) {
  // struct vmdisk_dev *devp = bdev->bd_disk->private_data;

  geo->cylinders = (NSECTORS & ~0x3f) >> 6;
  geo->heads = 4;
  geo->sectors = 16;
  geo->start = 4;

  return 0;
}

static struct block_device_operations vmdisk_ops = {
    .getgeo = vmdisk_getgeo,
};

static void setup_device(struct vmdisk_dev *devp) {
  memset(devp, 0, sizeof(struct vmdisk_dev));

  devp->size = NSECTORS * HARDSECT_SIZE;
  devp->data = vmalloc(devp->size);
  if (devp->data == NULL) {
    printk(KERN_NOTICE "vmalloc failure !!!\n");
    return;
  }

  spin_lock_init(&devp->lock);

  devp->queue = blk_alloc_queue(GFP_KERNEL);
  if (devp->queue == NULL) goto out_vfree;

  blk_queue_make_request(devp->queue, vmdisk_make_request);

  blk_queue_logical_block_size(devp->queue, HARDSECT_SIZE);
  devp->queue->queuedata = devp;

  devp->gd = alloc_disk(1);
  if (!devp->gd) {
    printk(KERN_NOTICE "alloc_disk failure\n");
    goto out_vfree;
  }

  devp->gd->major = VMDISK_MAJOR;
  devp->gd->first_minor = 0;
  devp->gd->fops = &vmdisk_ops;
  devp->gd->queue = devp->queue;
  devp->gd->private_data = devp;
  snprintf(devp->gd->disk_name, 32, "vmdisk");

  set_capacity(devp->gd, NSECTORS * (HARDSECT_SIZE / KERNEL_SECTOR_SIZE));
  add_disk(devp->gd);
  return;

out_vfree:
  if (devp->data) vfree(devp->data);
}

static int __init vmdisk_init(void) {
  int major = register_blkdev(VMDISK_MAJOR, "vmdisk");
  if (major <= 0) { return -EBUSY; }

  setup_device(&vmdisk_shared_data);
  return 0;
}
module_init(vmdisk_init);

static void __exit vmdisk_exit(void) {
  unregister_blkdev(VMDISK_MAJOR, "vmdisk");
}
module_exit(vmdisk_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wierton");
MODULE_DESCRIPTION("A simple vmem disk");
