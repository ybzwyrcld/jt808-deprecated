#include <jt808_service.h>

int main(int argc, char *argv[])
{
	jt808_service my_service;
	my_service.init(3398, 10);
	my_service.run(2000);

	return  0;
}

