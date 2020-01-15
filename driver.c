#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/delay.h>

#define REG_BASE 0x3f000000

#define GPIO_BASE (REG_BASE + 0x200000)
#define GPIO_SIZE 0xC0

#define PWM_BASE (REG_BASE + 0x20C000)
#define PWM_SIZE 0xC0

#define CLK_BASE (REG_BASE + 0x101000)
#define CLK_SIZE 0x100

#define MOT_DIR_PIN 24  
#define MOT_CLK_PIN 13

/* PWM Index */
#define PWM_CTRL 0x0
#define PWM_STA 0x4
#define PWM_DMAC 0x8
#define PWM_RNG1 0x10
#define PWM_DAT1 0x14
#define PWM_FIF1 0x18
#define PWM_RNG2 0x20
#define PWM_DAT2 0x24

#define PWM_BASECLK 9600000

/* Clock Offset */
#define CLK_PWM_INDEX 0xa0
#define CLK_PWMDIV_INDEX 0xa4

MODULE_AUTHOR("Ryotaro Hoshina");
MODULE_DESCRIPTION("driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

static dev_t dev;
static struct cdev cdv;
static struct class *cls = NULL;
static volatile u32 *gpio_base = NULL;
static volatile void __iomem *pwm_base = NULL;
static volatile void __iomem *clk_base = NULL;


void set_gpio(u32 mode, int number_of_bcm){

	u32 index, shift, mask;
	
	index = (u32)(number_of_bcm / 10);
	shift = (number_of_bcm % 10) * 3;
	mask =  ~(0x7 << shift);
	gpio_base[index] = (gpio_base[index] & mask) | (mode << shift);

}

void set_gpio_input(int number_of_bcm) {
	u32 mode = 0x00;
	set_gpio(mode, number_of_bcm);
}

void set_gpio_output(int number_of_bcm) {
	u32 mode = 0x01;
	set_gpio(mode, number_of_bcm);
}

void set_gpio_alt0(int number_of_bcm) {
	u32 mode = 0x04;
	set_gpio(mode, number_of_bcm);
}


static int getPWMCount(int freq)
{
	if (freq < 1)
		return PWM_BASECLK;
	if (freq > 10000)
		return PWM_BASECLK / 10000;

	return PWM_BASECLK / freq;
}

static void pwm_write32(uint32_t offset, uint32_t val)
{
	iowrite32(val, pwm_base + offset);
}

static void set_motor_freq(int freq)
{
	int dat;

	if (freq == 0) {
		set_gpio_output(MOT_CLK_PIN);
		return;
	} else {
		set_gpio_alt0(MOT_CLK_PIN);
	}

	if (freq > 0) {
		gpio_base[7] = 1 << MOT_DIR_PIN;
	} else {
		gpio_base[10] = 1 << MOT_DIR_PIN;
		freq = -freq;
	}
		
	dat = getPWMCount(freq);

	pwm_write32(PWM_RNG2, dat);
	pwm_write32(PWM_DAT2, dat >> 1);

	return;
}

static int parseFreq(const char __user *buf, size_t count, int *ret)
{
	char cval;
	int error = 0, i = 0, tmp, bufcnt = 0, freq;
	size_t readcount = count;
	int sgn = 1;

	char *newbuf = kmalloc(sizeof(char) * count, GFP_KERNEL);

	while (readcount > 0) {
		if (copy_from_user(&cval, buf + i, sizeof(char))) {
			kfree(newbuf);
			return -EFAULT;
		}

		if (cval == '-') {
			if (bufcnt == 0) {
				sgn = -1;
			}
		} else if (cval < '0' || cval > '9') {
			newbuf[bufcnt] = 'e';
			error = 1;
		} else {
			newbuf[bufcnt] = cval;
		}

		i++;
		bufcnt++;
		readcount--;

		if (cval == '\n') {
			break;
		}
	}

	freq = 0;
	for (i = 0, tmp = 1; i < bufcnt; i++) {
		char c = newbuf[bufcnt - i - 1];

		if (c >= '0' && c <= '9') {
			freq += (newbuf[bufcnt - i - 1] - '0') * tmp;
			tmp *= 10;
		}
	}

	*ret = sgn * freq;

	kfree(newbuf);

	return bufcnt;
}

static ssize_t motor_write(struct file* filp, const char* buf, size_t count, loff_t* pos)
{

	int freq, bufcnt;
	
	bufcnt = parseFreq(buf, count, &freq);
	gpio_base[7] = 1 << MOT_DIR_PIN;
	//gpio_base[10] = 1 << MOT_DIR_PIN;
	printk(KERN_INFO "count: %d  freq:%d\n", count, freq);
	set_motor_freq(freq);

	return bufcnt;
}


static struct file_operations driver_fops = {
        .owner = THIS_MODULE,
        .write = motor_write
};

static int gpio_map(void)
{
	gpio_base = ioremap_nocache(GPIO_BASE, GPIO_SIZE);
	pwm_base = ioremap_nocache(PWM_BASE, PWM_SIZE); 
	clk_base = ioremap_nocache(CLK_BASE, CLK_SIZE);

	iowrite32(0x5a000000 | (1 << 5), clk_base + CLK_PWM_INDEX);
	udelay(1000);

	iowrite32(0x5a000000 | (2 << 12), clk_base + CLK_PWMDIV_INDEX);
	iowrite32(0x5a000011, clk_base + CLK_PWM_INDEX);
	udelay(1000);

	return 0;
}
static int __init init_mod(void)
{
	int retval;

	gpio_map();

	set_gpio_output(MOT_DIR_PIN);
	set_gpio_alt0(MOT_CLK_PIN);
	
	pwm_write32(PWM_CTRL, 0x00008181);
	printk(KERN_DEBUG "pwm_ctrl:%08X\n", ioread32(pwm_base + PWM_CTRL));

	retval = alloc_chrdev_region(&dev, 0, 1, "driver");
	if(retval < 0){
		printk(KERN_ERR "alloc_chrdev_region failed.\n");
		return retval;
	}
	printk(KERN_INFO "%s is loaded. major:%d\n",__FILE__,MAJOR(dev));

	cdev_init(&cdv, &driver_fops);
        retval = cdev_add(&cdv, dev, 1);
        if(retval < 0){
                printk(KERN_ERR "cdev_add failed. major:%d, minor:%d",MAJOR(dev),MINOR(dev));
                return retval;
        }

        cls = class_create(THIS_MODULE,"driver");
        if(IS_ERR(cls)){
                printk(KERN_ERR "class_create failed.");
                return PTR_ERR(cls);
        }
	device_create(cls, NULL, dev, NULL, "driver%d",MINOR(dev));
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
