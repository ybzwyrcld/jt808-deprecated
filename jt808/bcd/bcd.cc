#include "bcd/bcd.h"

#include <string.h>


unsigned char BcdFromHex(const unsigned char &src) {
  unsigned char temp;

  temp = ((src / 10) << 4) + (src % 10);

  return temp;
}

unsigned char HexFromBcd(const unsigned char &src) {
  unsigned char temp;

  temp = (src >> 4)*10 + (src & 0x0f);
  return temp;
}

char *BcdFromStringCompress(const char *src, char *dst, const int &srclen) {
  char *ptr = dst;
  unsigned char temp;

  if (srclen % 2 != 0 ) {
    *ptr++ = BcdFromHex(*src++ - '0');
  }

  while (*src) {
    // *ptr++ = hex2bcd((*src++ - '0')*10 + (*src++ - '0'));
    temp = *src++ - '0';
    temp *= 10;
    temp += *src++ - '0';
    *ptr++ = BcdFromHex(temp);
  }

  *ptr = 0;

  return dst;
}

char *StringFromBcdCompress(const char *src, char *dst, const int &srclen) {
  char *ptr = dst;
  unsigned char temp;
  int cnt = srclen;

  while (cnt--) {
    temp = HexFromBcd(*src);
    *ptr++ = temp/10 + '0';
    if (dst[0] == '0')
      ptr = dst;
    *ptr++ = temp%10 + '0';
    ++src;
  }

  return dst;
}

