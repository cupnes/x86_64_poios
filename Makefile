DEPDIR=~/x86_64_poios

all: pdf

deploy: clean
	sudo chown -R yarakawa:yarakawa $(DEPDIR)
	cp config.yml catalog.yml *.re _cover.tex $(DEPDIR)/articles/

pdf: deploy
	cd $(DEPDIR) && ./build-in-docker.sh

clean:
	rm -rf *~ $(TARGET) book-pdf

.PHONY: deploy pdf clean
