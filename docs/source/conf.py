# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'ZBC Semihosting'
copyright = '2024, ZBC Project Contributors'
author = 'ZBC Project Contributors'
release = '0.1.0'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

# Set libclang path for sphinx-c-autodoc if specified via environment
import os
if 'LIBCLANG_PATH' in os.environ:
    import clang.cindex
    clang.cindex.Config.set_library_file(os.environ['LIBCLANG_PATH'])

extensions = [
    'sphinx_c_autodoc',
    'sphinx_c_autodoc.napoleon',
]

# -- Options for sphinx-c-autodoc --------------------------------------------

c_autodoc_roots = ['../../include']

templates_path = ['_templates']
exclude_patterns = []

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'sphinx_rtd_theme'
html_static_path = ['_static']

# -- Options for C domain ----------------------------------------------------

# Treat all unprefixed names as C
primary_domain = 'c'
highlight_language = 'c'
