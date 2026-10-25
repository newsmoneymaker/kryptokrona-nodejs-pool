# Changes

## 1.0.0 (2026-10-25)

* First release: Kryptokrona (XKR) pool, adapted from qwc-nodejs-pool (itself derived from veil-nodejs-pool
  and epic-nodejs-pool). Stratum for `cn_turtle_lite_slow_hash_v2` (`kryptokrona` in poolpayminer), plain
  cryptonote-nodejs-pool JSON-RPC (`login`/`job`/`submit`), share checking with the `xkrhash` helper
  (the coin's own real hash, vendored verbatim -- see `hasher/README.md`/kryptokrona-project.md).
* The node's `getblocktemplate` has no ready-to-hash field of its own (unlike more modern CryptoNote
  forks) -- `blobtool` (linked against the node's own compiled code, source in `blobtool/`) applies the
  real merge-mining-tag mutation and builds the correct hashing blob once per new template.
* The node's real RPC has no underscores (`getblocktemplate`/`getlastblockheader`/`submitblock`/
  `getblockheaderbyheight`) and its wallet-service is a modern, camelCase API
  (`getBalance`/`sendTransaction`/`getTransaction`/`getTransactionHashes`) -- NOT the Monero-style
  `get_transfers`/`transfer` the original QWC template assumed; every RPC call site was rewritten for this.
* Real single base58 address prefix (`SEKRe...`, 99 characters) -- this coin has no subaddress/
  integrated-address scheme at all, unlike Monero-derived coins.
* The variable difficulty is remembered per address and worker across reconnects
  (`poolServer.diffMemoryMinutes`, default 15): a miner whose connection is cut or renewed every half
  minute does not restart from the start difficulty each time.
* Real end-to-end tested against a live, fully-synced XKR mainnet node and poolpayminer: real shares
  accepted with no `--threads` override, on both Linux and Windows.
* Payments stay `dryRun` until a real block matures and the first real payout is verified end to end
  through the pool's own code (a real send was verified separately, as a one-off direct RPC call, see
  kryptokrona-project.md -- not yet exercised through `paymentProcessor.js` itself).
