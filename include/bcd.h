#ifndef __BCD_H__
#define __BCD_H__


unsigned char hex2bcd(const unsigned char &src);
unsigned char bcd2hex(const unsigned char &src);
unsigned char *str2bcd(const unsigned char *src, unsigned char *dst);

#endif // #ifndef __BCD_H__
