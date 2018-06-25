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


#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/mutex.h>

#include "translate.h"

#define MY_DEVICE_NAME "trans"

MODULE_DESCRIPTION("translate");
MODULE_AUTHOR("Janaina, Vadim");
MODULE_LICENSE("GPL");

static int rot = 3;
module_param(rot, int, 0);

/*Parameters*/
int trans_major = TRANSLATE_MAJOR_DYNAMIC;
int trans_minor = 0;
static int trans_nr_devs = TRANSLATE_NR_DEVS;
dev_t dev_num;
static int read = 0;
static int write = 0;
char buffer[40];

static struct mutex trans_mutex;
static struct cdev trans_cdev;

static int
trans_open(struct inode *inode, struct file *file) {
    mutex_lock(&trans_mutex);
	if(file->f_mode & FMODE_READ){
		if(read){
	mutex_unlock(&trans_mutex);
		return -EBUSY;
		}
		read = 1;
	}
	if(file->f_mode & FMODE_READ){
		if(write){
		mutex_unlock(&trans_mutex);
		return -EBUSY;
		}
		write = 1;
	}
	mutex_unlock(&trans_mutex);
	return 0;
}

static int
trans_close (struct inode * inode, struct file *file) {
	mutex_lock(&trans_mutex);
	if (file->f_mode & FMODE_READ)
		read = 0;
	if (file->f_mode & FMODE_WRITE)
		write = 0;
	mutex_unlock(&trans_mutex);
	return 0;
}


static ssize_t
trans_read(struct file *file, char __user *user_buffer, size_t size, loff_t *offset) {
    
    size_t bytes_not_copied;
    
    mutex_lock(&trans_mutex);
    PDEBUG("String to print: %s", buffer);
    
    bytes_not_copied = copy_to_user(user_buffer, buffer, strlen(buffer));

    PDEBUG("bytes_not_copied: %zu", bytes_not_copied);

    mutex_unlock(&trans_mutex);

    return size - bytes_not_copied;
}

static void
ceaser_encript (void) {
	char i = buffer[0];
	int c = 0;

	while (i != 0) {
		buffer[c] = buffer[c] + rot;
		c++;
		i = buffer[c];
	}
}

static ssize_t
trans_write (struct file * file, const char __user * user_buffer, size_t size, loff_t * offset) {
	mutex_lock(&trans_mutex);


	if (copy_from_user(buffer, user_buffer, size)) {
		PDEBUG("cannot copy all input to kernel buffer. Exiting");
		mutex_unlock(&trans_mutex);
		return -EFAULT;
	}

	ceaser_encript();

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
	dev_num = 0;
	printk(KERN_DEBUG "HERE I AM: %s:%i\n", __FILE__, __LINE__);
	err = alloc_chrdev_region(&dev_num, trans_minor, trans_nr_devs, MY_DEVICE_NAME);
    	trans_major = MAJOR(dev_num);
        
	if (err < 0) {
		PDEBUG("Unable to get chardevice region, error %d\n", err);
		return 0;
	}
	
    	mutex_init(&trans_mutex);
    
	cdev_init(&trans_cdev, &trans_fops);
	trans_cdev.owner = THIS_MODULE;
	err = cdev_add (&trans_cdev, dev_num, 1);
    
	/* Fail gracefully if need be */
	if (err)
		PDEBUG("Error %d adding trans", err);
	
	return 0; /* succeed */
}

static void
trans_exit(void) {
    	cdev_del(&trans_cdev);
	unregister_chrdev_region(dev_num, trans_nr_devs);
	mutex_trylock(&trans_mutex);
	mutex_unlock(&trans_mutex);
	mutex_destroy(&trans_mutex);
}

module_init(trans_init);
module_exit(trans_exit);
