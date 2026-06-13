# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'Zero Board Computer Reference'
copyright = '2024, ZBC Project Contributors'
author = 'ZBC Project Contributors'
release = '0.1.0'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    'breathe',
]

# -- Options for Breathe -----------------------------------------------------
#
# Doxygen writes XML to docs/build/doxygen/xml (see docs/Doxyfile).
# That directory is relative to docs/ (where `make html` runs), so from
# docs/source/ it is one level up.
breathe_projects = {
    'zbc': '../build/doxygen/xml',
}
breathe_default_project = 'zbc'

# Use :members: by default so doxygenclass/doxygenstruct emit members
# without restating it on every directive.
breathe_default_members = ('members',)

# README.md files inside _extra/ are dev-facing and must not ship to the
# rendered site; html_extra_path runs files through exclude_patterns before
# copying them.
exclude_patterns = ['**/README.md']

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'sphinx_rtd_theme'

# Standalone assets (e.g. the reveal.js intro deck) live under source/_extra/.
# Sphinx copies the contents of each listed directory into the build root, so
# the wrapper level here is what lets _extra/presentation-intro/ land at
# /presentation-intro/ in the published site.
html_extra_path = ['_extra']

# -- Options for the C / C++ domains -----------------------------------------

# Most API pages document C symbols, so default unqualified references
# to the C domain. cpp.rst sets ``.. default-domain:: cpp`` at the top
# to override locally.
primary_domain = 'c'
highlight_language = 'c'
