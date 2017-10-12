
all:
	gcc -Wall -Wextra -Werror -O3 -DHTTP_PARSER_STRICT=0 -c http_parser.c; \
	ar rcs libhttp_parser.a http_parser.o

clean:
	rm -f libhttp_parser.a http_parser.o

.PHONY: all clean
