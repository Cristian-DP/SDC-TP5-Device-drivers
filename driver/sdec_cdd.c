#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Catedra Sistemas de Computacion");
MODULE_DESCRIPTION("CDD TP5 - DHT11 Sensor Driver");

#define DEVICE_NAME     "sdec_cdd"
#define CLASS_NAME      "sdec"
#define DHT_GPIO        516

static dev_t first;
static struct cdev c_dev;
static struct class *cl;
static struct gpio_desc *dht_gpio_desc;

static int temperatura = 0;
static int humedad = 0;
static int senal_seleccionada = 0;
static spinlock_t datos_lock;
static struct timer_list dht_timer;

// ============================================================
// Protocolo DHT11
// ============================================================
static int dht11_read(int *temp, int *hum)
{
    int i, j;
    u8 datos[5] = {0, 0, 0, 0, 0};
    int timeout;

    gpiod_direction_output(dht_gpio_desc, 0);
    mdelay(18);
    gpiod_set_value(dht_gpio_desc, 1);
    udelay(40);
    gpiod_direction_input(dht_gpio_desc);

    timeout = 100;
    while (gpiod_get_value(dht_gpio_desc) == 1) {
        udelay(1);
        if (--timeout == 0) return -1;
    }

    timeout = 100;
    while (gpiod_get_value(dht_gpio_desc) == 0) {
        udelay(1);
        if (--timeout == 0) return -1;
    }

    timeout = 100;
    while (gpiod_get_value(dht_gpio_desc) == 1) {
        udelay(1);
        if (--timeout == 0) return -1;
    }

    for (i = 0; i < 5; i++) {
        for (j = 7; j >= 0; j--) {
            timeout = 100;
            while (gpiod_get_value(dht_gpio_desc) == 0) {
                udelay(1);
                if (--timeout == 0) return -1;
            }
            udelay(40);
            if (gpiod_get_value(dht_gpio_desc) == 1) {
                datos[i] |= (1 << j);
                timeout = 100;
                while (gpiod_get_value(dht_gpio_desc) == 1) {
                    udelay(1);
                    if (--timeout == 0) return -1;
                }
            }
        }
    }

    if (datos[4] != ((datos[0] + datos[1] + datos[2] + datos[3]) & 0xFF))
        return -2;

    *hum  = datos[0];
    *temp = datos[2];
    return 0;
}

// ============================================================
// Timer callback
// ============================================================
static void dht_timer_callback(struct timer_list *t)
{
    int temp, hum;
    unsigned long flags;

    if (dht11_read(&temp, &hum) == 0) {
        spin_lock_irqsave(&datos_lock, flags);
        temperatura = temp;
        humedad = hum;
        spin_unlock_irqrestore(&datos_lock, flags);
        printk(KERN_INFO "sdec_cdd: Temp=%d°C  Hum=%d%%\n", temp, hum);
    } else {
        printk(KERN_WARNING "sdec_cdd: Error leyendo DHT11\n");
    }

    mod_timer(&dht_timer, jiffies + HZ);
}

// ============================================================
// Operaciones del CDF
// ============================================================
static int my_open(struct inode *i, struct file *f)
{
    printk(KERN_INFO "sdec_cdd: open()\n");
    return 0;
}

static int my_close(struct inode *i, struct file *f)
{
    printk(KERN_INFO "sdec_cdd: close()\n");
    return 0;
}

static ssize_t my_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    char kbuf[32];
    int valor;
    unsigned long flags;
    int nbytes;

    if (*off > 0)
        return 0;

    spin_lock_irqsave(&datos_lock, flags);
    valor = (senal_seleccionada == 0) ? temperatura : humedad;
    spin_unlock_irqrestore(&datos_lock, flags);

    nbytes = snprintf(kbuf, sizeof(kbuf), "%d\n", valor);

    if (copy_to_user(buf, kbuf, nbytes))
        return -EFAULT;

    *off += nbytes;
    return nbytes;
}

static ssize_t my_write(struct file *f, const char __user *buf,
                        size_t len, loff_t *off)
{
    char kbuf[4];

    if (len > sizeof(kbuf) - 1)
        return -EINVAL;

    if (copy_from_user(kbuf, buf, len))
        return -EFAULT;

    kbuf[len] = '\0';

    if (kbuf[0] == '0') {
        senal_seleccionada = 0;
        printk(KERN_INFO "sdec_cdd: señal seleccionada = TEMPERATURA\n");
    } else if (kbuf[0] == '1') {
        senal_seleccionada = 1;
        printk(KERN_INFO "sdec_cdd: señal seleccionada = HUMEDAD\n");
    } else {
        return -EINVAL;
    }

    return len;
}

static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = my_open,
    .release = my_close,
    .read    = my_read,
    .write   = my_write,
};

// ============================================================
// Constructor y Destructor
// ============================================================
static int __init sdec_cdd_init(void)
{
    int ret;
    struct device *dev_ret;

    printk(KERN_INFO "sdec_cdd: Iniciando modulo TP5\n");

    // Obtener descriptor del GPIO usando el nuevo API
    dht_gpio_desc = gpio_to_desc(DHT_GPIO);
    if (!dht_gpio_desc) {
        printk(KERN_ERR "sdec_cdd: No se pudo obtener descriptor GPIO%d\n", DHT_GPIO);
        return -ENODEV;
    }

    spin_lock_init(&datos_lock);

    if ((ret = alloc_chrdev_region(&first, 0, 1, DEVICE_NAME)) < 0) {
        gpiod_put(dht_gpio_desc);
        return ret;
    }

    if (IS_ERR(cl = class_create(CLASS_NAME))) {
        unregister_chrdev_region(first, 1);
        gpiod_put(dht_gpio_desc);
        return PTR_ERR(cl);
    }

    if (IS_ERR(dev_ret = device_create(cl, NULL, first, NULL, DEVICE_NAME))) {
        class_destroy(cl);
        unregister_chrdev_region(first, 1);
        gpiod_put(dht_gpio_desc);
        return PTR_ERR(dev_ret);
    }

    cdev_init(&c_dev, &fops);
    if ((ret = cdev_add(&c_dev, first, 1)) < 0) {
        device_destroy(cl, first);
        class_destroy(cl);
        unregister_chrdev_region(first, 1);
        gpiod_put(dht_gpio_desc);
        return ret;
    }

    timer_setup(&dht_timer, dht_timer_callback, 0);
    mod_timer(&dht_timer, jiffies + 2 * HZ);

    printk(KERN_INFO "sdec_cdd: Modulo cargado. /dev/%s creado\n", DEVICE_NAME);
    return 0;
}

static void __exit sdec_cdd_exit(void)
{
    del_timer_sync(&dht_timer);
    cdev_del(&c_dev);
    device_destroy(cl, first);
    class_destroy(cl);
    unregister_chrdev_region(first, 1);
    gpiod_direction_input(dht_gpio_desc);
    gpiod_put(dht_gpio_desc);
    printk(KERN_INFO "sdec_cdd: Modulo descargado\n");
}

module_init(sdec_cdd_init);
module_exit(sdec_cdd_exit);