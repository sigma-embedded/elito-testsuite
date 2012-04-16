RUNTEST = @PKGLIBEXECDIR@/runtest

test-%:
	$(RUNTEST) --id "$(ID)" $(RUNTEST_OPTS) $(TESTDIR)/$* || :

category_start-%:
	@echo "========== $* =========="
