#ifndef COMMON_H_
#define COMMON_H_

/* common headers */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/slab.h>
#include <linux/uaccess.h>

/* metainfo */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Meng Xu");

/* complete the moduile */
#define REST_OF_MODULE                                      \
  static int __init prog_init(void) {                       \
    printk(KERN_INFO "module init");                        \
    return 0;                                               \
  }                                                         \
                                                            \
  static void __exit prog_exit(void) {                      \
    printk(KERN_INFO "module fini");                        \
  }                                                         \
                                                            \
  module_init(prog_init);                                   \
  module_exit(prog_exit);

#endif /* COMMON_H_ */
