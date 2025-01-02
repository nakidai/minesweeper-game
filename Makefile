CFLAGS += `pkg-config libbsd-overlay --cflags`
LDLIBS += `pkg-config libbsd-overlay --libs`

all: minesweeper-game

README: README.7
	mandoc -T ascii $< | col -b > $@

tags: minesweeper-game.c
	ctags minesweeper-game.c

install:
	install -d ${DESTDIR}/bin ${DESTDIR}/share/man/man6
	install -m755 minesweeper-game ${DESTDIR}/bin
	install -m644 minesweeper-game.6 ${DESTDIR}/share/man/man6

uninstall:
	${RM} ${DESTDIR}/bin/minesweeper-game
	${RM} ${DESTDIR}/share/man/man6/minesweeper-game.6

clean:
	rm -f minesweeper-game tags
