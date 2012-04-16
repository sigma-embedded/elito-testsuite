RUNTEST = ${pkglibexecdir}/runtest
PATH := ${pkglibexecdir}:${PATH}

export pkgdatadir pkglibexecdir pkglibdir TMPDIR

test-%:
	$(RUNTEST) --id "$(ID)" $(RUNTEST_OPTS) $(TESTDIR)/$* || :

category_start-%:
	@echo "========== $* =========="
