# Makefile
# Řešení IPK projekt 1, http resolver domenovych jmen
# Autor: Jan Doležel, FIT
# 2020

CC=gcc
FLAGS=-Wall -pedantic
.PHONY: run build

build: src/ipk_1.c
	$(CC) $(FLAGS) src/ipk_1.c -o ipk_1

run: ipk_1
	./ipk_1 $(PORT)
