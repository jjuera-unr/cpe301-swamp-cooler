#!/bin/sh
#
# compile-readme-pdf.sh
# A script to compile README.md into README-report.pdf using pandoc.

pandoc README.md -o README.pdf \
    --variable=fontfamily:arev \
    --toc \
    --highlight-style=tango \
    -V geometry:margin=1in
