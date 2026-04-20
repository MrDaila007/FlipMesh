# SPDX-License-Identifier: GPL-3.0-or-later
# Host-side unit tests (no uFBT). Run from repo root: make -C tests check

CC       := gcc
CFLAGS   := -std=c11 -Wall -Wextra -Werror -g -D_GNU_SOURCE
CPPFLAGS := -I stubs -I ../core -I ../core/lib/nanopb -I ../core/lib/meshtastic_api

CORE_SRC := ../core/fm_history.c ../core/fm_roster.c
STUB_SRC := stubs.c
TEST_SRC := test_main.c

.PHONY: all clean check

all: test_runner

test_runner: $(CORE_SRC) $(STUB_SRC) $(TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(CORE_SRC) $(STUB_SRC) $(TEST_SRC)

check: test_runner
	./test_runner

clean:
	rm -f test_runner
