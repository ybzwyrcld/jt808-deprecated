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

unsigned char *str2bcd(const unsigned char *src, unsigned char *dst)
{
	unsigned char *ptr = dst;

	if (strlen((char *)src) % 2 != 0 ) {
		*ptr++ = hex2bcd(*src++ - '0');
	}

	while (*src) {
		*ptr++ = hex2bcd((*src++ - '0')*10 + (*src++ - '0'));
	}

	*ptr = 0;

	return dst;
}

