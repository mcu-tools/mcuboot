- Update ptest to support test selection. Ptest can now be invoked with `list`
  to show the available tests and `run` to run them. The `-t` argument will
  select specific tests to run.
- Allow sim tests to skip slow tests.  By setting `MCUBOOT_SKIP_SLOW_TESTS` in
  the environment, the sim will skip two tests that are very slow.  In one
  instance this reduces the test time from 2 hours to about 5 minutes.  These
  slow tests are useful, in that they test bad powerdown recovery, but are
  inconvenient when testing other areas.
