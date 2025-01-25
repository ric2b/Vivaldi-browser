
all: emoji_presentation_scanner.c

%.c: %.rl Makefile
	ragel -F1 $<

CFLAGS= \
	-O2 \
	-Demoji_text_iter_t="char *" \
	-DEMOJI_LINKAGE= \
	$(NULL)

%.o: %.c Makefile
	$(CC) -c -o $@ $< $(CFLAGS)

size: emoji_presentation_scanner.o
	size $<

clean:
	rm -f emoji_presentation_scanner.o
