# syntax=docker/dockerfile:1
#FROM --platform=linux/amd64 emscripten/emsdk:3.1.65@sha256:f62da65f81994c85293222ea5f9d26203ffc34008194c329bfaaefcbb97d960b
FROM --platform=linux/amd64 emscripten/emsdk:3.1.40@sha256:c1e807a6e03ac5bd5b37bae2ace3c46c08579e2ddeb951037a3b8dac7067f2cc AS emsdk-base

RUN apt-get update
RUN apt-get install -y autoconf libtool build-essential automake libtool pkg-config meson ninja-build
# RUN curl -fsSL https://deb.nodesource.com/setup_20.x
# RUN apt-get install -y nodejs

WORKDIR /ogv.js

ENTRYPOINT make
