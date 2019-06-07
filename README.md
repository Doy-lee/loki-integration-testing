# Loki Integration Testing
A testing suite for running integration tests on Loki daemon. This is in heavy
development and is not reliable.

To run, build loki using `make integration` to build the integration binaries in
the integration sub directory of the build folder. Copy the binaries, `lokid`
and `loki-wallet-cli` into loki-integration-testing/bin/.

Change directory into bin, and execute `loki-integration-testing`. This will
dump blockchain files into the current working directory, so beware launching
the testing suite from other directories.

## Generating Blockchains
The integration framework can generate blockchains by programatically starting
up the clients and simulate a blockchain, outputting the results onto the disk.

Output files are placed in the working directory under the subdirectory
`./output/`. *Be warned* this deletes the output folder each invocation and
overwrites any existing files if there were any generated during blockchain
generation time.

For convenience some helper scripts are also generated to launch the clients.
Copy the scripts over to a non-integration mode binary for these to work.

For up to date information, pass the `--help` flag to the binaries to see the
possible customisations to the blockchain possible.

```
Integration Test Startup Flags

  --generate-blockchain         |                Generate a blockchain instead of running tests. Flags below are optionally specified.
    --service-nodes     <value> | (Default: 0)   How many service nodes to generate in the blockchain
    --daemons           <value> | (Default: 1)   How many normal daemons to generate in the blockchain
    --wallets           <value> | (Default: 1)   How many wallets to generate for the blockchain
    --wallet-balance    <value> | (Default: 100) How much Loki each wallet should have (non-atomic units)
    --fixed-difficulty  <value> | (Default: 1)   Blocks should be mined with set difficulty, 0 to use the normal difficulty algorithm
```

For example the line

```
./integration_test --generate-blockchain --service-nodes 10 --daemons 1 --wallets 2 --wallet-balance 300
```

Generates a blockchain on the testnet with 10 service nodes, 1 daemon, 2 wallets
each with atleast a balance of 300 Loki and generates a shell script per daemon
and wallet to launch a fully connected, closed off and private network.

```
./lokid --data-dir daemon_0 --fixed-difficulty 1 --p2p-bind-port 1111 --rpc-bind-port 2222 --zmq-rpc-bind-port 3333 --testnet --service-node --add-exclusive-node 127.0.0.1:1112 --add-exclusive-node 127.0.0.1:1113 --add-exclusive-node 127.0.0.1:1114 --add-exclusive-node 127.0.0.1:1115 --add-exclusive-node 127.0.0.1:1116 --add-exclusive-node 127.0.0.1:1117 --add-exclusive-node 127.0.0.1:1118 --add-exclusive-node 127.0.0.1:1119 --add-exclusive-node 127.0.0.1:1120 --add-exclusive-node 127.0.0.1:1121
```

## Notes
- On execution if you receive the error, "too many open files" when running the
  tests, you need to increase the file open limits which can be checked using
  `ulimit -a` (on Ubuntu).

  Limits can be adjusted in `/etc/security/limits.conf` by adding the line
  `<Username> soft nofile <Limit>`
  `<Username> hard nofile <Limit>`

  Where soft limit can increase up to the hard limit of files open.
