#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#define BASE_ADD	0x3f000000
#define BASE_ADD_GPIO	(0x200000 + BASE_ADD)
#define PIN_SW 	25
#define PIN_LED	24  

MODULE_AUTHOR("Ryotaro Hoshina");
MODULE_DESCRIPTION("driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

static dev_t dev;
static struct cdev cdv;
static struct class *cls = NULL;
static volatile u32 *gpio_base = NULL;


void set_gpio(u32 mode, int number_of_bcm){

	u32 index, shift, mask;
	gpio_base = ioremap_nocache(BASE_ADD_GPIO, 0xA0);
	
	index = (u32)(number_of_bcm / 10);
	shift = (number_of_bcm % 10) * 3;
	mask =  ~(0x7 << shift);
	gpio_base[index] = (gpio_base[index] & mask) | (mode << shift);

}

void set_gpio_input(int number_of_bcm) {
	u32 mode = 0x000;
	set_gpio(mode, number_of_bcm);
}

void set_gpio_output(int number_of_bcm) {
	u32 mode = 0x001;
	set_gpio(mode, number_of_bcm);
}

static ssize_t gpio_write(struct file* filp, const char* buf, size_t count, loff_t* pos)
{
	char c;
	if(copy_from_user(&c,buf,sizeof(char)))
		return -EFAULT;

	if(c == '0')
		gpio_base[10] = 1 << PIN_LED;
	else if(c == '1')
		gpio_base[7] = 1 << PIN_LED;

	return 1;
}

static ssize_t gpio_read(struct file* filp, char* buf, size_t count, loff_t* pos)
{
	int size = 0;
	char catch_string[] = {0x0, 0x0};
	int ret = ((gpio_base[13] & (0x01 << PIN_SW)) != 0);

	catch_string[0] = '0' + ret;

	catch_string[1] = '\n';
	if(copy_to_user(buf+size,(const char *)catch_string, sizeof(catch_string))){
		printk( KERN_INFO "copy_to_user failed\n" );
		return -EFAULT;
	}

	size += sizeof(catch_string);
	return size;
}

static struct file_operations led_fops = {
        .owner = THIS_MODULE,
        .write = gpio_write,
	.read = gpio_read
};

static int __init init_mod(void)
{
	int retval;

	set_gpio_output(PIN_LED);
	set_gpio_input(PIN_SW);

	retval = alloc_chrdev_region(&dev, 0, 1, "myled");
	if(retval < 0){
		printk(KERN_ERR "alloc_chrdev_region failed.\n");
		return retval;
	}
	printk(KERN_INFO "%s is loaded. major:%d\n",__FILE__,MAJOR(dev));

	cdev_init(&cdv, &led_fops);
        retval = cdev_add(&cdv, dev, 1);
        if(retval < 0){
                printk(KERN_ERR "cdev_add failed. major:%d, minor:%d",MAJOR(dev),MINOR(dev));
                return retval;
        }

        cls = class_create(THIS_MODULE,"myled");
        if(IS_ERR(cls)){
                printk(KERN_ERR "class_create failed.");
                return PTR_ERR(cls);
        }
	device_create(cls, NULL, dev, NULL, "myled%d",MINOR(dev));
	return 0;
}


static void __exit cleanup_mod(void)
{
	cdev_del(&cdv);
	device_destroy(cls, dev);
	class_destroy(cls);
	unregister_chrdev_region(dev, 1);
	printk(KERN_INFO "%s is unloaded. major:%d\n",__FILE__,MAJOR(dev));
}

module_init(init_mod);
module_exit(cleanup_mod);
