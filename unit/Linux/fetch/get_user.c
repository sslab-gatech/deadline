#include "common.h"

int handle(long dst, long __user *src) {
  get_user(dst, src);
  return 0;
}

REST_OF_MODULE
