.PHONY: all clean

INCS := -I../bootutil/include -Iinclude
DEFS := -DMCUBOOT_USE_MBED_TLS -DMCUBOOT_SIGN_RSA
FLAGS := -Wall -Werror

all: bootutil coverity

coverity: image_rsa.o image_validate.o caps.o bootutil_misc.o loader.o main.c keys.c
	gcc $(FLAGS) $(INCS) $(DEFS) -o $@ $^

bootutil: image_rsa.o image_validate.o caps.o bootutil_misc.o loader.o

image_rsa.o: ../bootutil/src/image_rsa.c
	gcc $(FLAGS) $(INCS) $(DEFS) -c -o $@ $^

image_ec.o: ../bootutil/src/image_ec.c
	gcc $(FLAGS) $(INCS) $(DEFS) -c -o $@ $^

image_ec256.o: ../bootutil/src/image_ec256.c
	gcc $(FLAGS) $(INCS) $(DEFS) -c -o $@ $^

image_validate.o: ../bootutil/src/image_validate.c
	gcc $(FLAGS) $(INCS) $(DEFS) -c -o $@ $^

caps.o: ../bootutil/src/caps.c
	gcc $(FLAGS) $(INCS) $(DEFS) -c -o $@ $^

bootutil_misc.o: ../bootutil/src/bootutil_misc.c
	gcc $(FLAGS) $(INCS) $(DEFS) -c -o $@ $^

loader.o: ../bootutil/src/loader.c
	gcc $(FLAGS) $(INCS) $(DEFS) -c -o $@ $^

clean:
	rm -f *.o coverity
