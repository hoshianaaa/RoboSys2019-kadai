#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/delay.h>

#define BASE_ADD	0x3f000000
#define BASE_ADD_GPIO	(0x200000 + BASE_ADD)
#define MOTOR_DIR_PIN	24  

#define PWMCLK_BASE 13

/* Pwm Addr */
#define PWM_OFFSET 0x20C000
#define PWM_SIZE 0xC0
#define PWM_BASE (BASE_ADD + PWM_OFFSET)

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

/* Clock Addr */
#define CLK_OFFSET 0x101000
#define CLK_SIZE 0x100
#define CLK_BASE (BASE_ADD + CLK_OFFSET)

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
	gpio_base = ioremap_nocache(BASE_ADD_GPIO, 0xA0);
	
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
	pwm_base = ioremap_nocache(PWM_BASE, PWM_SIZE); 
	iowrite32(val, pwm_base + offset);
}

static void set_pwm_freq(int freq)
{
	int dat;

	dat = getPWMCount(freq);
	pwm_write32(PWM_RNG2, dat);
	pwm_write32(PWM_DAT2, dat >> 1);

	return;
}

static ssize_t gpio_write(struct file* filp, const char* buf, size_t count, loff_t* pos)
{
	char c;
	if(copy_from_user(&c,buf,sizeof(char)))
		return -EFAULT;

	if(c == '0'){
		gpio_base[7] = 1 << MOTOR_DIR_PIN;
		set_pwm_freq(10);
	}
	else if(c == '1'){
		gpio_base[7] = 1 << MOTOR_DIR_PIN;
		set_pwm_freq(50);
	}
	else if(c == '2'){
		set_pwm_freq(100);
		gpio_base[7] = 1 << MOTOR_DIR_PIN;
	}

	return 1;
}


static struct file_operations driver_fops = {
        .owner = THIS_MODULE,
        .write = gpio_write,
};

static int __init init_mod(void)
{
	int retval;

	clk_base = ioremap_nocache(CLK_BASE, CLK_SIZE);

	iowrite32(0x5a000000 | (1 << 5), clk_base + CLK_PWM_INDEX);
	udelay(1000);

	iowrite32(0x5a000000 | (2 << 12), clk_base + CLK_PWMDIV_INDEX);
	iowrite32(0x5a000011, clk_base + CLK_PWM_INDEX);
	
	udelay(1000);

	set_gpio_output(MOTOR_DIR_PIN);
	set_gpio_alt0(PWMCLK_BASE);
	
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
