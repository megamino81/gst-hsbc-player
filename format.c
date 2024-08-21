#include <stdio.h>

#define fourcc_code(a, b, c, d) ((__u32)(a) | ((__u32)(b) << 8) | \
                 ((__u32)(c) << 16) | ((__u32)(d) << 24))

typedef unsigned int __u32;

void 
print_char(__u32 format) 
{
	unsigned char a,b,c,d;
	a = (format & 0x000000ff);
	b = (format & 0x0000ff00) >> 8;
	c = (format & 0x00ff0000) >> 16;
	d = (format & 0xff000000) >> 24;

	printf("%c %c %c %c\n", a, b, c, d);
}

int main(int argc, char *argv[]) 
{
	int argb8888 = fourcc_code('A', 'B', '2', '4');
	int value = 1498831189;

	printf("argb8888=%d\n", argb8888);
	print_char(argb8888);

	printf("value=%d\n", value);
	print_char(value);

	return 0;
}
