# SPDX-License-Identifier: GPL-2.0
# Makefile for LED triggers tools

CFLAGS = -Wall -Wextra -g -I../../driver/include/uapi

all: uledtriggers
%: %.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	$(RM) uledtriggers

.PHONY: all clean
