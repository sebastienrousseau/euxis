# Minimal makefile for Sphinx documentation

SPHINXOPTS    ?=
SPHINXBUILD   ?= sphinx-build
SOURCEDIR     = docs
BUILDDIR      = _build

.PHONY: help clean html livehtml

help:
	@echo "Euxis Documentation Build"
	@echo ""
	@echo "Usage:"
	@echo "  make html      Build HTML documentation"
	@echo "  make clean     Remove build artifacts"
	@echo "  make livehtml  Start live-reload server"
	@echo ""

clean:
	rm -rf $(BUILDDIR)

html:
	$(SPHINXBUILD) -b html $(SOURCEDIR) $(BUILDDIR)/html $(SPHINXOPTS)
	@echo ""
	@echo "Build finished. HTML pages are in $(BUILDDIR)/html."

livehtml:
	sphinx-autobuild $(SOURCEDIR) $(BUILDDIR)/html $(SPHINXOPTS)
