prefix = /usr/local
INSTALL= /usr/bin/install -c
GMSGFMT = /usr/bin/msgfmt
MSGFMT = /usr/bin/msgfmt
MSGMERGE = /usr/bin/msgmerge
XGETTEXT = /usr/bin/xgettext

INSTALLNLSDIR=$(DESTDIR)${prefix}/share/locale

PACKAGE = muse

CATALOGS = it.mo es.mo

POTFILES=../src/gtkgui2/about.cpp \
	 ../src/gtkgui2/docking.cpp \
	 ../src/gtkgui2/encoder.cpp \
	 ../src/gtkgui2/filedump.cpp \
	 ../src/gtkgui2/gchan.cpp \
	 ../src/gtkgui2/gchan_events.cpp \
	 ../src/gtkgui2/ice.cpp \
	 ../src/gtkgui2/musegtk.cpp \
	 ../src/gtkgui2/utils.cpp \
	 ../src/gtkgui2/vumeters.cpp

all: $(PACKAGE).pot $(CATALOGS)

$(PACKAGE).pot: $(POTFILES)
	${XGETTEXT} --default-domain=$(PACKAGE) \
       	  --add-comments --keyword=_ --keyword=N_ $(POTFILES)
	if cmp -s $(PACKAGE).po $(PACKAGE).pot; then \
	  rm -f $(PACKAGE).po; \
	else \
	  mv $(PACKAGE).po $(PACKAGE).pot; \
	fi

update-po: $(PACKAGE).pot
	for cat in $(CATALOGS); do \
	  lang=`echo $$cat | sed 's/.mo$$//'`; \
	  mv $$lang.po $$lang.old.po; \
	  echo "$$lang:"; \
	  if ${MSGMERGE} $$lang.old.po $(PACKAGE).pot -o $$lang.po; then \
	    rm -f $$lang.old.po; \
	  else \
    	    echo "msgmerge for $$cat failed!"; \
	    rm -f $$lang.po; mv $$lang.old.po $$lang.po; \
	  fi; \
	done

%.mo: %.po
	${GMSGFMT} --statistics --check --verbose --output-file=$@ $<

clean:
	rm -f *.mo

distclean: clean
	rm -f whois.pot

install: $(CATALOGS)
	for n in $(CATALOGS); do \
	    l=`basename $$n .mo`; \
	    $(INSTALL) -m 755 -d $(INSTALLNLSDIR)/$$l; \
	    $(INSTALL) -m 755 -d $(INSTALLNLSDIR)/$$l/LC_MESSAGES; \
	    $(INSTALL) -m 644 $$n $(INSTALLNLSDIR)/$$l/LC_MESSAGES/$(PACKAGE).mo; \
	done

