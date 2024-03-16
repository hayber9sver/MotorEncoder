/*
 * devone.c
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/gpio.h> 
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/timekeeping.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <asm/uaccess.h>

MODULE_LICENSE("Dual BSD/GPL");

static int devone_devs = 1;        /* device count */
static int devone_major = 0;       /* MAJOR: dynamic allocation */
static int devone_minor = 0;       /* MINOR: static allocation */
static struct cdev devone_cdev;
static struct class *devone_class = NULL;
static dev_t devone_dev;

#define OPENCODER_CPR 112
#define GPIO_1 229 //gpio 1
#define BUFFER_SIZE 10
static const char DeviceName[] = "devonedev";
static int gpio_1_irg;
struct tasklet_struct mytask = {0};
spinlock_t gpio_lock, read_lock;

struct RawRotatryInfo{
    int count;
    struct timespec64 tv;
};
struct RotatryInfo{
    int count;
    int64_t time;
};
struct RawRotatryInfo rotatry_info, irq_data;
struct RotatryInfo read_data[10];

static int data_index;

void tasklet_handler(unsigned long data)
{
    unsigned long flags, flags_read;
    struct RawRotatryInfo tmp;
    {
        spin_lock_irqsave(&gpio_lock, flags);
        tmp = rotatry_info;
        spin_unlock_irqrestore(&gpio_lock, flags);
    }
    {
        spin_lock_irqsave(&read_lock, flags_read);
        read_data[data_index].count = rotatry_info.count;
        read_data[data_index].time = timespec64_to_ns(&rotatry_info.tv);

        spin_unlock_irqrestore(&read_lock, flags_read);
    }
    ++data_index;
    if(data_index >= BUFFER_SIZE)
        data_index = 0;

}
static irqreturn_t gpio_handler(int irq, void *dev_id) 
{
    // TODO: I need to add buffer pool & spin_lock_irq to handle this situation.
    // Because irq_top & irp_bottom is different frequency, the data was modified by irq_top 
    // before irq_bottom load data maybe.
    unsigned long flags;

    ktime_get_boottime_ts64(&irq_data.tv);
    ++irq_data.count;
    if(irq_data.count >= OPENCODER_CPR) irq_data.count = 0; 

    spin_lock_irqsave(&gpio_lock, flags);
    rotatry_info = irq_data;
    spin_unlock_irqrestore(&gpio_lock, flags);
    tasklet_schedule(&mytask);
    return IRQ_HANDLED;
}

int devone_open(struct inode *inode, struct file *file_ptr)
{
  printk(KERN_NOTICE "encoder_open, version\n");
  return 0;
}

ssize_t devone_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    int retval;
    unsigned long flags;
    struct RotatryInfo tmp[BUFFER_SIZE];
    spin_lock_irqsave(&read_lock, flags);
    memcpy(tmp, read_data, sizeof(struct RotatryInfo) * BUFFER_SIZE);
    spin_unlock_irqrestore(&read_lock, flags);

    if(count > BUFFER_SIZE)
    {
        retval = -EFAULT;
        goto out;
    }
    if (copy_to_user(buf, tmp, count * sizeof(struct RotatryInfo))) {
        retval = -EFAULT;
        goto out;
    }
    retval = count;
out:
    printk("read failed");
    return (retval);
}

struct file_operations devone_fops = {
    .open = devone_open,
    .read = devone_read
};

static int
pinInit(int pin, irqreturn_t (*gpio_irq)(int, void *))
{
    int irqNum, rv;

    if (! gpio_is_valid(pin)) {
        printk(KERN_NOTICE "Encoder GPIO pin %d not valid\n", pin);
        return -1;
    }
    if (gpio_request(pin, "Encoder pinInit()") < 0) {
        printk(KERN_NOTICE "Encoder GPIO request denied, pin %d\n", pin);
        return -1;
    }
    if (gpio_direction_input(pin) == -ENOSYS)
    {
        printk(KERN_ERR "Gpio %d direction have limitation \n", pin);
        return -1;
    }
    irqNum = gpio_to_irq(pin);
    if (irqNum == -EINVAL)
    {
        printk(KERN_ERR "Gpio %d direction can't set irq \n", pin);
        return -1;
    }
    rv = request_irq(irqNum,
            (void *) gpio_irq,
            IRQF_TRIGGER_RISING,
            DeviceName,
            NULL);
    if (rv != 0) {
        printk(KERN_ERR "%s: request_irq(%d) failed (%d)\n", DeviceName, pin, rv);
        return -1;
    }

    printk(KERN_NOTICE "Setting finish, pin %d\n", pin);

    return irqNum;
}


static int devone_init(void)
{
    dev_t dev = MKDEV(devone_major, 0);
    int alloc_ret = 0;
    int major;
    int cdev_err = 0;
    struct device *class_device = NULL;

    alloc_ret = alloc_chrdev_region(&dev, 0, devone_devs, "devone");
    if (alloc_ret)
        goto error;
    devone_major = major = MAJOR(dev);

    cdev_init(&devone_cdev, &devone_fops);
    devone_cdev.owner = THIS_MODULE;
    devone_cdev.ops = &devone_fops;
    cdev_err = cdev_add(&devone_cdev, MKDEV(devone_major, devone_minor), 1);
    if (cdev_err)
        goto error;

    /* register class */
    devone_class = class_create(THIS_MODULE, "devone");
    if (IS_ERR(devone_class)) {
        goto error;
    }
    devone_dev = MKDEV(devone_major, devone_minor);
    class_device = device_create(
                    devone_class,
                    NULL,
                    devone_dev,
                    NULL,
                    DeviceName);
    if(class_device == NULL)
    {
        printk(KERN_ALERT "device_create error. \r");
        goto error;
        
    }

    printk(KERN_ALERT "devone driver(major %d) installed.\n", major);

    data_index = 0;
    irq_data =(struct RawRotatryInfo){0 ,(struct timespec64){0 , 0}};
    rotatry_info =(struct RawRotatryInfo){0 ,(struct timespec64){0 , 0}};
    spin_lock_init(&gpio_lock);
    spin_lock_init(&read_lock);
    tasklet_init(&mytask, tasklet_handler, 0); //define static time member and update it value when irq trigger
    gpio_1_irg = pinInit(GPIO_1, gpio_handler);
    if(gpio_1_irg == -1) goto error;


    return 0;

error:
    if(class_device == NULL)
        class_destroy(devone_class);

    if (cdev_err == 0)
        cdev_del(&devone_cdev);

    if (alloc_ret == 0)
        unregister_chrdev_region(dev, devone_devs);
    
    gpio_free(GPIO_1);

    return -1;
}

static void devone_exit(void)
{
    dev_t dev = MKDEV(devone_major, 0);

    /* unregister class */
    device_destroy(devone_class, devone_dev);
    class_destroy(devone_class);

    cdev_del(&devone_cdev);
    unregister_chrdev_region(dev, devone_devs);

    free_irq(gpio_1_irg, NULL);
    gpio_free(GPIO_1);
    tasklet_kill(&mytask);
    printk(KERN_ALERT "devone driver removed.\n");

}

module_init(devone_init);
module_exit(devone_exit);