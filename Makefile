all: ibc

ibc:
	clang -framework IOKit -Os src/ibc.c -o ibc

clean:
	rm -f ibc
