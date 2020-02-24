#include <core/string.h>

int main(void)
{
	int size = 0x1000;

	memset(0x82000000, 0, size);

	return 0;
}
