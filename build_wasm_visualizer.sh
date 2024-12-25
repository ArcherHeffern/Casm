#!/usr/bin/env bash

rm -rf web
mkdir web

emcc -o web/casm.html \
src/visualizer_main.c src/ui.c src/util.c src/casm.c src/preprocess.c src/lexer.c src/animation.c src/complex_animations.c src/future.c src/render.c src/style_overrides.c \
-Wall \
-std=c99 \
-D_DEFAULT_SOURCE \
-Wno-missing-braces \
-Wunused-result \
-Os \
-I ~/Desktop/code/casm/raylib-dev_webassembly/include \
-L ~/Desktop/code/casm/raylib-dev_webassembly/lib \
-lraylib.web \
-s USE_GLFW=3 \
-s ASYNCIFY \
-s TOTAL_MEMORY=67108864 \
-s FORCE_FILESYSTEM=1 \
-DPLATFORM_WEB \
--shell-file='./web_template.html' \
-s EXPORTED_FUNCTIONS='["_free","_malloc","_main"]' \
-s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]'
