#ifndef __BCD_H__
#define __BCD_H__


unsigned char hex2bcd(const unsigned char &src);
unsigned char bcd2hex(const unsigned char &src);
unsigned char *str2bcd_compress(const unsigned char *src, unsigned char *dst);
unsigned char *bcd2str_compress(const unsigned char *src, unsigned char *dst, const int &srclen);

#endif // #ifndef __BCD_H__
