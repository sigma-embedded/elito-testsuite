RUNTEST = @PKGLIBEXECDIR@/runtest

test-%:
	$(RUNTEST) $(RUNTEST_OPTS) $(TESTDIR)/$*

category_start-%:
	@echo "========== $* =========="
