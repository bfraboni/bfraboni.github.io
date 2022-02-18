#!/bin/bash
# pdf2thumb.sh
convert -density 300 -resize 10% -filter Sinc -strip ${1}[0] -background white -alpha remove -alpha off ${2}