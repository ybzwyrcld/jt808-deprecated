#ifndef BCD_H
#define BCD_H


unsigned char BcdFromHex(const unsigned char &src);
unsigned char HexFromBcd(const unsigned char &src);
char *BcdFromStringCompress(const char *src, char *dst, const int &srclen);
char *StringFromBcdCompress(const char *src, char *dst, const int &srclen);

#endif // BCD_H
