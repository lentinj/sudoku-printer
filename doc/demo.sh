#!/bin/sh
set -eu
INPUT="$1"

ffmpeg -y -i "${INPUT}" -vf "vidstabdetect=shakiness=10:accuracy=15:stepsize=32:result=transforms.trf" -f null -
ffmpeg -y -i "${INPUT}" -vf "vidstabtransform=smoothing=1000,fps=5,rotate=-90*PI/180,crop=1080:1080,scale=540:540" demo.mp4
rm transforms.trf
mpv demo.mp4
