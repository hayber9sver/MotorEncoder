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
#include <linux/device.h>
#include <asm/uaccess.h>

MODULE_LICENSE("Dual BSD/GPL");

static int devone_devs = 1;        /* device count */
static int devone_major = 0;       /* MAJOR: dynamic allocation */
static int devone_minor = 0;       /* MINOR: static allocation */
static struct cdev devone_cdev;
static struct class *devone_class = NULL;
static dev_t devone_dev;

#define GPIO_1 229 //gpio 1
static const char DeviceName[] = "devonedev";
static int gpio_1_irg;

static irqreturn_t gpio_irq(int irq, void *dev_id) {
    

    int volt = gpio_get_value(GPIO_1);
    return IRQ_HANDLED;
}

ssize_t devone_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    int i;
    unsigned char val = 0xc0;
    int retval;
    //dump_stack();

    for (i = 0 ; i < count ; i++) {
        if (copy_to_user(&buf[i], &val, 1)) {
            retval = -EFAULT;
            goto out;
        }
    }
    retval = count;
out:
    printk("read failed");
    return (retval);
}

struct file_operations devone_fops = {
    .read = devone_read,
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
            IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
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

    gpio_1_irg = pinInit(GPIO_1, gpio_irq);
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

    printk(KERN_ALERT "devone driver removed.\n");

}

module_init(devone_init);
module_exit(devone_exit);