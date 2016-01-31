all:
	cd perl && $(MAKE)
	cd src && $(MAKE)

install: all
	cd perl && $(MAKE) install
	cd src && $(MAKE) install
	
	mkdir -p "$(DESTDIR)/etc/nag2mqtt"
	cp ex/nag2mqtt/nag2mqtt.conf ex/nag2mqtt/neb2mqtt.cfg "$(DESTDIR)/etc/nag2mqtt/"
	
	mkdir -p "$(DESTDIR)/usr/lib/tmpfiles.d"
	cp ex/tmpfiles.d/nag2mqtt.conf "$(DESTDIR)/usr/lib/tmpfiles.d/"

clean:
	cd perl && $(MAKE)
	cd src && $(MAKE)
