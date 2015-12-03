# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.  This file is offered as-is,
# without any warranty.

PREFIX = /usr
BIN = /bin
DATA = /share
BINDIR = $(PREFIX)$(BIN)
DATADIR = $(PREFIX)$(DATA)
DOCDIR = $(DATADIR)/doc
INFODIR = $(DATADIR)/info
MANDIR = $(DATADIR)/man
MAN1DIR = $(MANDIR)/man1
LICENSEDIR = $(DATADIR)/licenses

PKGNAME = librarian
COMMAND = librarian


OPTIMISE = -Og -g
WARN = -Wall -Wextra -Wdouble-promotion -Wformat=2 -Winit-self -Wmissing-include-dirs \
       -Wtrampolines -Wfloat-equal -Wshadow -Wmissing-prototypes -Wmissing-declarations \
       -Wredundant-decls -Wnested-externs -Winline -Wno-variadic-macros -Wsign-conversion \
       -Wswitch-default -Wconversion -Wsync-nand -Wunsafe-loop-optimizations -Wcast-align \
       -Wstrict-overflow -Wdeclaration-after-statement -Wundef -Wbad-function-cast \
       -Wcast-qual -Wwrite-strings -Wlogical-op -Waggregate-return -Wstrict-prototypes \
       -Wold-style-definition -Wpacked -Wvector-operation-performance \
       -Wunsuffixed-float-constants -Wsuggest-attribute=const -Wsuggest-attribute=noreturn \
       -Wsuggest-attribute=pure -Wsuggest-attribute=format -Wnormalized=nfkc -pedantic
#OPTIMISE = -O2
#WARN = -Wall -Wextra -pedantic
FLAGS = -std=c99 $(WARN) $(OPTIMISE)



.PHONY: default
default: base info shell

.PHONY: all
all: base doc shell

.PHONY: bas
base: cmd

.PHONY: command
cmd: bin/librarian

bin/librarian: obj/librarian.o
	@mkdir -p bin
	${CC} ${FLAGS} -o $@ $^ ${LDFLAGS}

obj/%.o: src/%.c
	mkdir -p obj
	${CC} ${FLAGS} -c -o $@ ${CPPFLAGS} ${CFLAGS} $<

.PHONY: doc
doc: info pdf dvi ps

.PHONY: info
info: bin/librarian.info
bin/%.info: doc/info/%.texinfo
	@mkdir -p bin
	$(MAKEINFO) $<
	mv $*.info $@

.PHONY: pdf
pdf: bin/librarian.pdf
bin/%.pdf: doc/info/%.texinfo
	@! test -d obj/pdf || rm -rf obj/pdf
	@mkdir -p bin obj/pdf
	cd obj/pdf && texi2pdf ../../"$<" < /dev/null
	mv obj/pdf/$*.pdf $@

.PHONY: dvi
dvi: bin/librarian.dvi
bin/%.dvi: doc/info/%.texinfo
	@! test -d obj/dvi || rm -rf obj/dvi
	@mkdir -p bin obj/dvi
	cd obj/dvi && $(TEXI2DVI) ../../"$<" < /dev/null
	mv obj/dvi/$*.dvi $@

.PHONY: ps
ps: bin/librarian.ps
bin/%.ps: doc/info/%.texinfo
	@! test -d obj/ps || rm -rf obj/ps
	@mkdir -p bin obj/ps
	cd obj/ps && texi2pdf --ps ../../"$<" < /dev/null
	mv obj/ps/$*.ps $@

.PHONY: shell
shell: bash fish zsh

.PHONY: bash
bash: bin/librarian.bash-completion

.PHONY: fish
fish: bin/librarian.fish-completion

.PHONY: zsh
zsh: bin/librarian.zsh-completion

obj/librarian.auto-completion: src/librarian.auto-completion
	@mkdir -p obj
	cp $< $@
	sed -i 's/^(librarian$$/($(COMMAND)/' $@

bin/librarian.%sh-completion: obj/librarian.auto-completion
	@mkdir -p bin
	auto-auto-complete $*sh --output $@ --source $<



.PHONY: install
install: install-base install-info install-man install-shell

.PHONY: install-all
install-all: install-base install-doc install-shell

.PHONY: install-base
install-base: install-cmd install-copyright

.PHONY: install-cmd
install-cmd: bin/librarian
	install -dm755 -- "$(DESTDIR)$(BINDIR)"
	install -m755 $< -- "$(DESTDIR)$(BINDIR)/$(COMMAND)"

.PHONY: install-copyright
install-copyright: install-license

.PHONY: install-license
install-license:
	install -dm755 -- "$(DESTDIR)$(LICENSEDIR)/$(PKGNAME)"
	install -m644 LICENSE -- "$(DESTDIR)$(LICENSEDIR)/$(PKGNAME)"

.PHONY: install-doc
install-doc: install-info install-pdf install-dvi install-ps install-man

.PHONY: install-info
install-info: bin/librarian.info
	install -dm755 -- "$(DESTDIR)$(INFODIR)"
	install -m644 $< -- "$(DESTDIR)$(INFODIR)/$(PKGNAME).info"

.PHONY: install-pdf
install-pdf: bin/librarian.pdf
	install -dm755 -- "$(DESTDIR)$(DOCDIR)"
	install -m644 $< -- "$(DESTDIR)$(DOCDIR)/$(PKGNAME).pdf"

.PHONY: install-dvi
install-dvi: bin/librarian.dvi
	install -dm755 -- "$(DESTDIR)$(DOCDIR)"
	install -m644 $< -- "$(DESTDIR)$(DOCDIR)/$(PKGNAME).dvi"

.PHONY: install-ps
install-ps: bin/librarian.ps
	install -dm755 -- "$(DESTDIR)$(DOCDIR)"
	install -m644 $< -- "$(DESTDIR)$(DOCDIR)/$(PKGNAME).ps"

.PHONY: install-man
install-man: doc/man/librarian.1
	install -dm755 -- "$(DESTDIR)$(MAN1DIR)"
	install -m644 $< -- "$(DESTDIR)$(MAN1DIR)/$(COMMAND).1"

.PHONY: install-shell
install-shell: install-bash install-fish install-zsh

.PHONY: install-bash
install-bash: bin/librarian.bash-completion
	install -dm755 -- "$(DESTDIR)$(DATADIR)/bash-completion/completions"
	install -m644 $< -- "$(DESTDIR)$(DATADIR)/bash-completion/completions/$(COMMAND)"

.PHONY: install-fish
install-fish: bin/librarian.fish-completion
	install -dm755 -- "$(DESTDIR)$(DATADIR)/fish/completions"
	install -m644 $< -- "$(DESTDIR)$(DATADIR)/fish/completions/$(COMMAND).fish"

.PHONY: install-zsh
install-zsh: bin/librarian.zsh-completion
	install -dm755 -- "$(DESTDIR)$(DATADIR)/zsh/site-functions"
	install -m644 $< -- "$(DESTDIR)$(DATADIR)/zsh/site-functions/_$(COMMAND)"



.PHONY: uninstall
uninstall:
	-rm -- "$(DESTDIR)$(BINDIR)/$(COMMAND)"
	-rm -- "$(DESTDIR)$(LICENSEDIR)/$(PKGNAME)/LICENSE"
	-rmdir -- "$(DESTDIR)$(LICENSEDIR)/$(PKGNAME)"
	-rm -- "$(DESTDIR)$(INFODIR)/$(PKGNAME).info"
	-rm -- "$(DESTDIR)$(DOCDIR)/$(PKGNAME).pdf"
	-rm -- "$(DESTDIR)$(DOCDIR)/$(PKGNAME).dvi"
	-rm -- "$(DESTDIR)$(DOCDIR)/$(PKGNAME).ps"
	-rm -- "$(DESTDIR)$(MAN1DIR)/$(COMMAND).1"
	-rm -- "$(DESTDIR)$(DATADIR)/bash-completion/completions/$(COMMAND)"
	-rm -- "$(DESTDIR)$(DATADIR)/fish/completions/$(COMMAND).fish"
	-rm -- "$(DESTDIR)$(DATADIR)/zsh/site-functions/_$(COMMAND)"



.PHONY: clean
clean:
	-rm -r bin obj

