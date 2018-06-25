/* my_chardevice.c
 *
 * Copyright 2018 Vadim Budagov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/mutex.h>
#include <linux/string.h>

#include "translate.h"

#define MY_DEVICE_NAME "trans"

MODULE_DESCRIPTION("translate");
MODULE_AUTHOR("Janaina, Vadim");
MODULE_LICENSE("GPL");

static int rot = 3;
module_param(rot, int, 0);

/*Parameters*/
int trans_major = TRANSLATE_MAJOR_DYNAMIC;
static int trans_nr_devs = TRANSLATE_NR_DEVS;

dev_t dev_num = 0;

struct trans {
    int minor;
    int read;
    int write;
    char buffer[40];
    struct cdev cdev;
};

static struct trans * trans0;
static struct trans * trans1;

static struct mutex trans_mutex;
static struct trans *trans_data;

static int
trans_open(struct inode *inode, struct file *file) {
    PDEBUG("open \n");
    mutex_lock(&trans_mutex);

    trans_data = container_of(inode->i_cdev, struct trans, cdev);

    file->private_data = trans_data;

    if(file->f_mode & FMODE_READ){
        if(trans_data->read){
            mutex_unlock(&trans_mutex);
            return -EBUSY;
        }
        trans_data->read = 1;
    }
    if(file->f_mode & FMODE_READ){
        if(trans_data->write){
            mutex_unlock(&trans_mutex);
            return -EBUSY;
        }
        trans_data->write = 1;
    }


    mutex_unlock(&trans_mutex);
    return 0;
}

static int
trans_close (struct inode * inode, struct file *file) {
    PDEBUG("close \n");
    mutex_lock(&trans_mutex);
    trans_data = (struct trans *)file->private_data;

    if (file->f_mode & FMODE_READ)
        trans_data->read = 0;

    if (file->f_mode & FMODE_WRITE)
        trans_data->write = 0;

    mutex_unlock(&trans_mutex);
    return 0;
}


static ssize_t
trans_read(struct file *file, char __user *user_buffer, size_t size, loff_t *offset) {
    size_t bytes_not_copied;
    PDEBUG("read \n");
    trans_data = (struct trans *)file->private_data;
     
    mutex_lock(&trans_mutex);
    PDEBUG("String to print: %s", trans_data.buffer);
    
    bytes_not_copied = copy_to_user(user_buffer, trans_data->buffer, strlen(trans_data->buffer));

    PDEBUG("bytes_not_copied: %zu", bytes_not_copied);

    mutex_unlock(&trans_mutex);

    return size - bytes_not_copied;
}

static void
ceaser_encript (struct trans* dev) {
    char i = dev->buffer[0];
    int c = 0;
    PDEBUG("MINOR-> %d", minor);

    while (i != 0) {
	if (dev->minor == 0) {
            dev->buffer[c] = dev->buffer[c] + rot;
	} else {
            dev->buffer[c] = dev->buffer[c] - rot;
	}
        c++;
        i = dev->buffer[c];
    }
}

static ssize_t
trans_write (struct file * file, const char __user * user_buffer, size_t size, loff_t * offset) {
    PDEBUG("write \n");
    mutex_lock(&trans_mutex);

    trans_data = (struct trans *)file->private_data;

    if (copy_from_user(trans_data->buffer, user_buffer, size)) {
        PDEBUG("cannot copy all input to kernel buffer. Exiting");
        mutex_unlock(&trans_mutex);
        return -EFAULT;

    }

    ceaser_encript(trans_data);

    mutex_unlock(&trans_mutex);
    return size;
}



const struct file_operations trans_fops = {
    .owner = THIS_MODULE,
    .open = trans_open,
    .read = trans_read,
    .write = trans_write,
    .release = trans_close
};

static int
trans_init(void) {
    int err;
    PDEBUG("Init\n");

    err = alloc_chrdev_region(&dev_num, trans0->minor, trans_nr_devs, MY_DEVICE_NAME);
    trans_major = MAJOR(dev_num);
        
    if (err < 0) {
        PDEBUG("Unable to get chardevice region, error %d\n", err);
        return err;
    }

    mutex_init(&trans_mutex);
    
    trans0 = (struct trans *) kmalloc(sizeof(struct trans *), GFP_KERNEL);
    if (!trans0) {
        PDEBUG("kmalloc trans0 error\n");
        return -ENOMEM;
    }
    trans1 = (struct trans *) kmalloc(sizeof(struct trans *), GFP_KERNEL);
    if (!trans1) {
        PDEBUG("kmalloc trans1 error\n");
        return -ENOMEM;
    }
    

    cdev_init(&trans0->cdev, &trans_fops);
    trans0->cdev.owner = THIS_MODULE;
    trans0->minor = 0;
    trans0->read = 0;
    trans0->write = 0;
    memset (trans0->buffer, 0, sizeof(trans0->buffer));
    err = cdev_add (&trans0->cdev, dev_num, 1);

    if (err) {
        PDEBUG("Error %d adding trans0", err);
    }

    cdev_init(&trans1->cdev, &trans_fops);
    trans1->cdev.owner = THIS_MODULE;
    trans1->minor = 0;
    trans1->read = 0;
    trans1->write = 0;
    memset (trans1->buffer, 0, sizeof(trans1->buffer));
    dev_num = MKDEV(trans_major, trans1->minor);
    err = cdev_add (&trans1->cdev, dev_num, 1);

    if (err) {
        PDEBUG("Error %d adding trans1", err);
    }

    return 0;
}

static void
trans_exit(void) {
    PDEBUG("Exit \n");
    cdev_del(&trans0->cdev);
    cdev_del(&trans1->cdev);

    dev_num = MKDEV(trans_major, trans0->minor);
    unregister_chrdev_region(dev_num, trans_nr_devs);

    kfree(trans0);
    kfree(trans1);

    mutex_trylock(&trans_mutex);
    mutex_unlock(&trans_mutex);
    mutex_destroy(&trans_mutex);
}

module_init(trans_init);
module_exit(trans_exit);
