#include <string.h>

#include <bcd.h>

unsigned char hex2bcd(const unsigned char &src)
{
	unsigned char temp;

	temp = ((src / 10) << 4) + (src % 10);

	return temp;
}

unsigned char bcd2hex(const unsigned char &src)
{
	unsigned char temp;

	temp = (src >> 4)*10 + (src & 0x0f);
	return temp;
}

unsigned char *str2bcd_compress(const unsigned char *src, unsigned char *dst)
{
	unsigned char *ptr = dst;
	unsigned char temp;

	if (strlen((char *)src) % 2 != 0 ) {
		*ptr++ = hex2bcd(*src++ - '0');
	}

	while (*src) {
		//*ptr++ = hex2bcd((*src++ - '0')*10 + (*src++ - '0'));
		temp = *src++ - '0';
		temp *= 10;
		temp += *src++ - '0';
		*ptr++ = hex2bcd(temp);
	}

	*ptr = 0;

	return dst;
}

unsigned char *bcd2str_compress(const unsigned char *src, unsigned char *dst, const int &srclen)
{
	unsigned char *ptr = dst;
	unsigned char temp;
	unsigned int cnt = srclen;

	while (cnt--) {
		temp = bcd2hex(*src);
		*ptr++ = temp/10 + '0';
		if (dst[0] == '0')
			ptr = dst;
		*ptr++ = temp%10 + '0';
		++src;
	}

	return dst;
}

