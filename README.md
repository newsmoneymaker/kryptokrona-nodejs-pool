# kryptokrona-nodejs-pool

Mining pool software for **Kryptokrona (XKR)** written in Node.js: a stratum server for Kryptokrona's own
`cn_turtle_lite_slow_hash_v2` (CryptoNight-family, 256 KB scratchpad) with share checking, block accounting
through the pool wallet, and batch payouts. It is a fork of [cryptonote-nodejs-pool](https://github.com/dvandal/cryptonote-nodejs-pool)
by Dvandal (GNU GPL v2) and of its adaptations [epic-nodejs-pool](https://github.com/newsmoneymaker/epic-nodejs-pool),
[veil-nodejs-pool](https://github.com/newsmoneymaker/veil-nodejs-pool) and [qwc-nodejs-pool](https://github.com/newsmoneymaker/qwc-nodejs-pool).
Live example: <https://xkrona.pool-pay.com>.

## What it does

* **Stratum server** (plain TCP and TLS ports, the plain cryptonote-nodejs-pool JSON-RPC shape -- `login`/`job`/`submit`): the pool asks the
  node for a block template (`getblocktemplate`), has `blobtool` (a small C++ helper linked against the node's own compiled code, see
  `kryptokrona-project.md`) apply the required merge-mining-tag mutation and build the real hashing blob, gives every miner that blob and a
  share target that follows the miner's hashrate (vardiff), and checks every share itself with a small C++ helper (`hasher/xkrhash`):
  `cn_turtle_lite_slow_hash_v2` of the hashing blob. A share that meets the network difficulty is submitted to the node with `submitblock`.
  **Use poolpayminer 1.1.32+ with `-a kryptokrona`** -- this is NOT the same algorithm as poolpayminer's own `cn-pico`/`cryptonight-turtle`
  despite similar names and matching memory/iteration parameters (confirmed by cross-checking against the real node's own code).
* **Accounts** are Kryptokrona addresses (`SEKRe...`, 99 characters, Keccak checksum verified -- this coin has no separate subaddress/integrated-address prefix at all, only the one standard type),
  optionally `+worker`, with a reward mode prefix `prop:` (shared, default) or `solo:`.
* **Rewards:** PROP with time weighting (slush) or SOLO.
* **Block unlocker:** a block is settled after `depth` blocks (the real coinbase unlock window is 20 blocks; the pool waits longer to be
  safe). The reward is what the pool wallet received in the block's coinbase transaction (wallet-service `getTransaction` on the block's
  `miner_tx_hash`); a block that is no longer on the chain is marked orphaned and nothing is credited.
* **Payment processor:** everyone who is due is paid in **one wallet `sendTransaction` per round** (the network fee is shared by the paid
  miners or paid by the pool). The balance is debited before sending; a batch whose outcome is unknown (crash, timeout) is found again among
  the wallet's recent transactions on its own address and never sent twice; refused or failed batches go back to the balances. Dry-run mode,
  a whitelist for rehearsals and an emergency brake (`deployment/pause-payments.sh`).
* **Website and API:** a ready website (`website_example/`) with the dashboard and its four graphs, blocks, payments, top miners, worker statistics, a
  "Getting started" page with a config generator, and the public read-only JSON API.
* **Protection against connection floods:** limits per IP, a login deadline, an optional IP allow list, banning of miners with many invalid shares.

## Developer donation (please read)

The pool takes a **developer donation** from the reward of every block it finds, before the miners' shares are computed. It is configured in
`config.json` (see `config_examples/kryptokrona.json`):

```json
"blockUnlocker": {
  "poolFee": 1,
  "donations": { "XKRYzAiBotA5xs5GwM4C5ax4bkxSxztLSyetGYUEnZkJQRw7Yws9wXbp7kY1F3bQpb9gYtTziL5CFPKUJD24Jvb9ESscmv4os": 0.5 }
}
```

* `poolFee` is the fee of the pool operator (percent). `donations` is a table `Kryptokrona address -> percent` (up to 10% per entry) for the developers of
  this software; **the default is 0.5% to the address of the project's own pool**. The donation is paid like any other balance.
* It is included in the "Pool Fee" figure that the pool's website and API show to the miners (as in the original software).
* You are free to change the percentage, the address or to empty the table (`"donations": {}`): it is your pool and the license is the GPL.
  Please tell your miners the truth about the fees of your pool.
* This has nothing to do with the miner poolpayminer (a separate project with its own fee).

## Installation

See [docs/INSTALL.md](docs/INSTALL.md): the Kryptokrona node and wallet-service (build from source), `blobtool`, the `xkrhash` helper, Redis, the pool
services (systemd templates in `deployment/`), the website and the first payout rehearsal.

Requirements: Linux, Node.js 18 or newer, Redis, a Kryptokrona node (`kryptokronad`, built from
[github.com/kryptokrona/kryptokrona](https://github.com/kryptokrona/kryptokrona)) synchronised with the network and its own `kryptokrona-service`
wallet-service with the pool wallet, a C++17 compiler + Boost (for `blobtool`, linked against the node's own compiled code) and an AES-NI capable CPU
(for the `xkrhash` helper), a web server for the website and a TLS certificate for the TLS stratum ports.

## Miners

**Stock [XMRig](https://github.com/xmrig/xmrig) does NOT support this algorithm.** Use
[poolpayminer](https://github.com/newsmoneymaker/poolpayminer) 1.1.32+ (Windows and Linux, free, has its own fee) instead -- it is NOT the same algorithm
as poolpayminer's own built-in `cn-pico`/`cryptonight-turtle` either, despite similar names (see kryptokrona-project.md). Example:
`poolpayminer -o xkrona.pool-pay.com:5301 --tls -u YOUR_XKR_ADDRESS+rigname -p x -a kryptokrona`.

## Tests

```
npm install
node test/test-account.js                 # address handling (base58 + Keccak checksum), needs nothing else
```

## Money warning

The payment processor moves real coins. Rehearse first: `"dryRun": true`, then a whitelist (`onlyAccounts`) with a few small payouts of your own,
then enable it. A transaction that has been sent to the network can not be cancelled. Keep the wallet's mnemonic seed backup private -- and note that any address created with the wallet-service's plain `createAddress` (no arguments) is an independently generated keypair, NOT derived from that seed: back up its own secret spend key (`getSpendKeys`) separately, or it cannot be recovered even with the seed restored.

## License and credits

GNU GPL v2 (see [LICENSE](LICENSE)). Based on cryptonote-nodejs-pool, Copyright (c) Dvandal and contributors. Kryptokrona itself is a separate program and is
not included.
