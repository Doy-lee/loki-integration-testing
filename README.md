# Loki Integration Testing
A testing suite for running integration tests on Loki daemon. This is in heavy
development and is not reliable.

To run, build loki using `make integration` to build the integration binaries in
the integration sub directory of the build folder. Copy the binaries, `lokid`
and `loki-wallet-cli` into loki-integration-testing/bin/.

Change directory into bin, and execute `loki-integration-testing`. This will
dump blockchain files into the current working directory, so beware launching
the testing suite from other directories.

## Notes
- On execution if you receive the error, "too many open files" when running the
  tests, you need to increase the file open limits which can be checked using
  `ulimit -a` (on Ubuntu).

  Limits can be adjusted in `/etc/security/limits.conf` by adding the line
  `<Username> soft nofile <Limit>`
  `<Username> hard nofile <Limit>`

  Where soft limit can increase up to the hard limit of files open.
