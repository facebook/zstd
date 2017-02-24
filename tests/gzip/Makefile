# ################################################################
# Copyright (c) 2017-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.
# ################################################################

PRGDIR = ../../programs
VOID   = /dev/null


.PHONY: all
all: test-gzip-env test-helin-segv test-hufts test-keep test-list test-memcpy-abuse test-mixed test-null-suffix-clobber test-stdin test-trailing-nul test-unpack-invalid test-zdiff test-zgrep-context test-zgrep-f test-zgrep-signal test-znew-k test-z-suffix 
	@echo Testing completed

.PHONY: zstd
zstd:
	$(MAKE) -C $(PRGDIR) zstd
	#alias gzip='$(PRGDIR)/zstd --format=gzip'
	#ln -sf /drive_d/GitHub/zstd/programs/zstd /usr/local/bin/gzip
	gzip --version

.PHONY: clean
clean:
	@$(MAKE) -C $(PRGDIR) $@ > $(VOID)
	@$(RM) *.trs *.log
	@echo Cleaning completed


#------------------------------------------------------------------------------
# validated only for Linux, OSX, Hurd and some BSD targets
#------------------------------------------------------------------------------
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU FreeBSD DragonFly NetBSD))
#.PHONY: test-gzip-env test-helin-segv test-hufts test-keep test-list test-memcpy-abuse test-mixed test-null-suffix-clobber test-stdin test-trailing-nul test-unpack-invalid test-zdiff test-zgrep-context test-zgrep-f test-zgrep-signal test-znew-k test-z-suffix 

test-%: zstd
	@./test-driver.sh --test-name $* --log-file $*.log --trs-file $*.trs --expect-failure "no" --color-tests "yes" --enable-hard-errors "yes" ./$* || echo error
#|| exit 1

endif
