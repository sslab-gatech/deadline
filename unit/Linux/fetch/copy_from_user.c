#include "common.h"

int handle(void *dst, void __user *src, int len) {
  copy_from_user(dst, src, len);
  return 0;
}

REST_OF_MODULE
