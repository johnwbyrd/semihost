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

templates_path = ['_templates']
exclude_patterns = []

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'sphinx_rtd_theme'
html_static_path = ['_static']

# -- Options for the C / C++ domains -----------------------------------------

# Most API pages document C symbols, so default unqualified references
# to the C domain. cpp.rst sets ``.. default-domain:: cpp`` at the top
# to override locally.
primary_domain = 'c'
highlight_language = 'c'
