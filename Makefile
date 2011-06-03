all: encapsulate

encapsulate: encapsulate.c
	gcc -o encapsulate encapsulate.c

install:
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp encapsulate $(DESTDIR)$(PREFIX)/bin
	chown root:root $(DESTDIR)$(PREFIX)/bin/encapsulate
	chmod u+s $(DESTDIR)$(PREFIX)/bin/encapsulate
