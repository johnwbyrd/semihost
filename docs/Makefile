# Minimal makefile for Sphinx documentation
#

# You can set these variables from the command line, and also
# from the environment for the first two.
SPHINXOPTS    ?=
SPHINXBUILD   ?= sphinx-build
SOURCEDIR     = source
BUILDDIR      = build

# Put it first so that "make" without argument is like "make help".
help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

.PHONY: help Makefile doxygen html

# Run Doxygen to emit XML for Breathe to ingest. The XML lands in
# build/doxygen/xml, which conf.py points breathe_projects at.
# mkdir is needed because Doxygen will only create the leaf of
# OUTPUT_DIRECTORY, not its parents -- so on a clean checkout where
# build/ does not exist yet, Doxygen fails to create build/doxygen/.
doxygen:
	mkdir -p build/doxygen
	doxygen Doxyfile

# Explicit html target so Doxygen runs before Sphinx. Other targets
# (linkcheck, clean, latexpdf, ...) still hit the catch-all below.
html: doxygen
	@$(SPHINXBUILD) -M html "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%: Makefile
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
