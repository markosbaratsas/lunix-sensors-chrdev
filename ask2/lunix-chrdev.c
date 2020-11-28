/*
 * lunix-chrdev.c
 *
 * Implementation of character devices
 * for Lunix:TNG
 *
 * < Your name here >
 *
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mmzone.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>

#include "lunix.h"
#include "lunix-chrdev.h"
#include "lunix-lookup.h"

/*
 * Global data
 */
struct cdev lunix_chrdev_cdev;
/*
 * Just a quick [unlocked] check to see if the cached
 * chrdev state needs to be updated from sensor measurements.
 */
static int lunix_chrdev_state_needs_refresh(struct lunix_chrdev_state_struct *state)
{
	struct lunix_sensor_struct *sensor;
	
	WARN_ON ( !(sensor = state->sensor));
	/* ? */
	if(sensor->msr_data[state->type]->last_update > state->buf_timestamp)//new data
		return 1;
	/* The following return is bogus, just for the stub to compile */
	return 0; /* no new data */
}

/*
 * Updates the cached state of a character device
 * based on sensor data. Must be called with the
 * character device state lock held.
 */
static int lunix_chrdev_state_update(struct lunix_chrdev_state_struct *state)
{
	struct lunix_sensor_struct *sensor;
	uint32_t value;
	long lookup;
	
	//debug("leaving\n");
	WARN_ON ( !(sensor = state->sensor));
	if(!lunix_chrdev_state_needs_refresh(state))
			return -EAGAIN;
	/*
	 * Grab the raw data quickly, hold the
	 * spinlock for as little as possible.
	 */
	/* ? */
	/* Why use spinlocks? See LDD3, p. 119 */
	spin_lock(&sensor->lock);
	value = sensor->msr_data[state->type]->values[0];
	state->buf_timestamp = sensor->msr_data[state->type]->last_update;
	spin_unlock(&sensor->lock);
	/*
	 * Any new data available?
	 */
	/* ? */

	/*
	 * Now we can take our time to format them,
	 * holding only the private state semaphore
	 */
	lookup = 0; //initialization for compile
	if(state->type == 0)
		lookup = lookup_voltage[value];
	else if(state->type == 1)
		lookup = lookup_temperature[value];
	else if(state->type == 2)
		lookup = lookup_light[value];
	if(lookup<0)
    		state->buf_lim = sprintf((char*)state->buf_data, "%ld,%ld\n", lookup/1000, -(lookup%1000)/10);
  	else
    		state->buf_lim = sprintf((char*)state->buf_data, "%ld,%ld\n", lookup/1000, (lookup%1000)/10);
	/* ? */
	debug("leaving with update\n");
	return 0;
}

/*************************************
 * Implementation of file operations
 * for the Lunix character device
 *************************************/

static int lunix_chrdev_open(struct inode *inode, struct file *filp)
{
	/* Declarations */
	/* ? */
	int ret;
	unsigned int minor;
	struct lunix_chrdev_state_struct *lunix_chrdev_state; //define a new linux character device private state structure
	
	debug("entering OPEN\n");
	ret = -ENODEV;
	if ((ret = nonseekable_open(inode, filp)) < 0)
		goto out;

	/*
	 * Associate this open file with the relevant sensor based on
	 * the minor number of the device node [/dev/sensor<NO>-<TYPE>]
	 */
	minor = iminor(inode);
	//filp->private_data = &lunix_sensors[minor/8]; //xwris auto to cat trwei killed, me afto apla den emfanizei tipota odigos sel 19
	
	/* Allocate a new Lunix character device private state structure */
	lunix_chrdev_state = kzalloc(sizeof(*lunix_chrdev_state), GFP_KERNEL);
	lunix_chrdev_state->type = minor%8;
	lunix_chrdev_state->sensor = &lunix_sensors[minor/8];
	sema_init(&lunix_chrdev_state->lock, 1);
	lunix_chrdev_state->buf_lim = 0;
	lunix_chrdev_state->buf_timestamp = 0;
	filp->private_data = lunix_chrdev_state;
	/* ? */
out:
	debug("leaving, with ret = %d\n", ret);
	return ret;
}

static int lunix_chrdev_release(struct inode *inode, struct file *filp)
{
	kfree(filp->private_data); //deallocate linux character device state allocated in open
	/* ? */
	return 0;
}

static long lunix_chrdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	/* Why? */
	return -EINVAL;
}

static ssize_t lunix_chrdev_read(struct file *filp, char __user *usrbuf, size_t cnt, loff_t *f_pos)
{
	ssize_t ret;

	struct lunix_sensor_struct *sensor;
	struct lunix_chrdev_state_struct *state;

	state = filp->private_data;
	WARN_ON(!state);

	sensor = state->sensor;
	WARN_ON(!sensor);
	
	debug("Entering read\n");
	/* Lock? */
	if(down_interruptible(&state->lock))
		return -ERESTARTSYS;
	debug("Locked device successful\n");
	/*
	 * If the cached character device state needs to be
	 * updated by actual sensor data (i.e. we need to report
	 * on a "fresh" measurement, do so
	 */
	if (*f_pos == 0) {
		while (lunix_chrdev_state_update(state) == -EAGAIN) {
			/* ? */
			/* The process needs to sleep */
			/* See LDD3, page 153 for a hint */
			debug("I'm in while\n");
			up(&state->lock);
			wait_event_interruptible(sensor->wq, lunix_chrdev_state_needs_refresh(state));
			debug("I'm down the wait\n");
			if(down_interruptible(&state->lock))
				return -ERESTARTSYS;
		}
	}

	/* End of file */
	if(*f_pos > state->buf_lim){
		ret = 0;
		goto out;
	}
	/* ? */
	
	/* Determine the number of cached bytes to copy to userspace */
	/* ? */
	cnt = min(cnt, (size_t)(state->buf_lim - *f_pos));
	if(copy_to_user(usrbuf, state->buf_data, cnt)){
		up(&state->lock);
		return -EFAULT;
	}
	*f_pos += cnt;
	ret = cnt;
	/* Auto-rewind on EOF mode? */
	if(*f_pos >= state->buf_lim)
		*f_pos = 0;
	/* ? */
out:
	/* Unlock? */
	up(&state->lock);
	return ret;
}

static int lunix_chrdev_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return -EINVAL;
}

static struct file_operations lunix_chrdev_fops = 
{
        .owner          = THIS_MODULE,
	.open           = lunix_chrdev_open,
	.release        = lunix_chrdev_release,
	.read           = lunix_chrdev_read,
	.unlocked_ioctl = lunix_chrdev_ioctl,
	.mmap           = lunix_chrdev_mmap
};

int lunix_chrdev_init(void)
{
	/*
	 * Register the character device with the kernel, asking for
	 * a range of minor numbers (number of sensors * 8 measurements / sensor)
	 * beginning with LINUX_CHRDEV_MAJOR:0
	 */
	int ret;
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;
	
	debug("initializing character device\n");
	cdev_init(&lunix_chrdev_cdev, &lunix_chrdev_fops);
	lunix_chrdev_cdev.owner = THIS_MODULE;
	
	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	/* ? */
	/* register_chrdev_region? */
	ret = register_chrdev_region(dev_no, lunix_minor_cnt, "LUNIX SENSORS");
	if (ret < 0) {
		debug("failed to register region, ret = %d\n", ret);
		goto out;
	}	

	/* ? */
	/* cdev_add? */
	ret = cdev_add(&lunix_chrdev_cdev, dev_no, lunix_minor_cnt);
	if (ret < 0) {
		debug("failed to add character device\n");
		goto out_with_chrdev_region;
	}
	debug("completed successfully\n");
	return 0;

out_with_chrdev_region:
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
out:
	return ret;
}

void lunix_chrdev_destroy(void)
{
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;
		
	debug("entering\n");
	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	cdev_del(&lunix_chrdev_cdev);
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
	debug("leaving\n");
}
