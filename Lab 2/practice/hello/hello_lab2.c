#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

static int __init hello_lab2_init(void)
{
    pr_info("hello_lab2: module loaded\n");
    return 0;
}

static void __exit hello_lab2_exit(void)
{
    pr_info("hello_lab2: module unloaded\n");
}

module_init(hello_lab2_init);
module_exit(hello_lab2_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LOS201: practice module lifecycle");
