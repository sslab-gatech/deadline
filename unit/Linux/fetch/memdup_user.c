#include "common.h"

int handle(void *dst, void __user *src, int len) {
  dst = memdup_user(src, len);
  return 0;
}

REST_OF_MODULE
