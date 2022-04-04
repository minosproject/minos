all: mkrmd

CFLAGS := -Wall -g -D_XOPEN_SOURCE -D_GNU_SOURCE \
	-Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing \
	-fno-common -Werror-implicit-function-declaration -Wno-format-overflow \
	-Wno-format-security -I../../generic/include -MD -MP

mkrmd: mkrmd.c
	@ echo "        Building mkrmd ..."
	@ gcc mkrmd.c -o mkrmd $(CFLAGS)

.PHONY: clean

clean:
	@rm -f mkrmd.o mkrmd mkrmd.d

-include mkrmd.d
