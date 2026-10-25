# Installing a Kryptokrona pool

Paths below are examples: the pool in `/opt/kryptokrona-nodejs-pool`, the node and wallet in `/opt/kryptokrona`, all run by the user
`xkrpool` (the systemd templates in `deployment/systemd/` use these paths).

## 1. Kryptokrona node and wallet

Build the node and wallet-service from source (C++17, Boost, the project's own vendored dependencies):

```
git clone --recursive https://github.com/kryptokrona/kryptokrona https://github.com/kryptokrona/kryptokrona-nodejs-pool/raw/stub && cd kryptokrona
mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j4 Daemon WalletService
# binaries in build/src/: kryptokronad, kryptokrona-service
```

Run the node (`deployment/systemd/xkr-node.service`):
`kryptokronad --data-dir /opt/kryptokrona/data --rpc-bind-ip 127.0.0.1 --rpc-bind-port 11898 --p2p-bind-port 11897`.
It finds its peers by itself and synchronises over time; check with
`curl -s 127.0.0.1:11898/getinfo` (look for `"synced": true`). Open the P2P port 11897 in your firewall for incoming peers.

Create the pool wallet with the wallet-service's own JSON-RPC (`createAddress`), then note the returned address -- it is the pool
address (`poolServer.poolAddress`): every block template pays its coinbase there. **Ask the wallet-service for the wallet's mnemonic
seed (`getMnemonicSeed`) and write it down somewhere safe and offline right away** -- do not let it only ever exist inside the
running wallet-service.

Then run the wallet-service (`deployment/systemd/xkr-wallet-rpc.service`): `kryptokrona-service --wallet-file /opt/kryptokrona/wallet/pool
--password-file /opt/kryptokrona/wallet/password --rpc-bind-ip 127.0.0.1 --rpc-bind-port 41899 --disable-rpc-login
--daemon-address 127.0.0.1:11898 --trusted-daemon`. It listens on the local machine only and pays without a password: keep only the
coins that are about to be paid out in this wallet.

If the node and the pool run on different machines, forward both RPC ports over SSH instead of exposing them (this daemon's own RPC
has no authentication at all) -- see `kryptokrona-project.md` in this project's own notes for the exact systemd unit used in
production (`xkr-tunnel.service`, two `-L` forwards over one SSH connection).

## 2. blobtool

This daemon's `getblocktemplate` does not hand back a ready-to-hash blob (unlike more modern Monero-derived daemons): the node's own
reference miner always rewrites the coinbase transaction's `extra` field with a merge-mining tag before computing the real hash
(`src/Miner/MinerManager.cpp`'s `adjustMergeMiningTag()`), even though this coin does not actually merge-mine anything. `blobtool`
(source alongside this project, see `kryptokrona-project.md`) links directly against the node's own compiled
`libCryptoNoteCore.a`/`libSerialization.a`/`libCommon.a`/`libCrypto.a`/`libLogging.a` to do this exactly as the real node does,
instead of a hand-rolled (and easy to get subtly wrong) reimplementation. Build it against your own node's source/build tree and
point `config.json`'s `blobtool.remotePath` (plus `blobtool.sshHost`, or `127.0.0.1` if it runs locally) at the resulting binary.
It only runs once per new block template (every `poolServer.templateRefresh` seconds), not per share, so an SSH round trip is fine.

## 3. Hasher

The helper (`hasher/xkrhash`) is this coin's own real `cn_turtle_lite_slow_hash_v2`, vendored verbatim from the node's own
`src/crypto/` (symbols renamed `xkr_` to avoid any collision with another vendored CryptoNote-family hash elsewhere on the same
machine -- see `hasher/xkr-src/xkr_rename.h`). Build it with `cd hasher && make` (needs a CPU with AES-NI). No key/dataset setup is
needed, unlike a RandomX-based pool's own hasher.

## 4. Redis

Use a dedicated instance with a password and AOF (`deployment/redis-pool.conf.example`, unit `xkr-pool-redis`, port 6403).

## 5. The pool

```
cd /opt/kryptokrona-nodejs-pool && npm install --production
cp config_examples/kryptokrona.json config.json      # then edit it
```

Edit `config.json`: `poolHost`, `poolServer.poolAddress`, the ports and the certificate for TLS (`poolServer.sslCert/sslKey`), `redis`,
`api.password`, `blockUnlocker.poolFee` and `donations`, `payments`, `node`, `wallet`, `hasher.path`, `blobtool`.
**Keep `payments.dryRun: true` until the rehearsal below.**

```
cp deployment/systemd/*.service /etc/systemd/system/ && systemctl daemon-reload
systemctl enable --now xkr-pool-redis xkr-node xkr-wallet-rpc
systemctl enable --now xkr-pool xkr-pool-api xkr-pool-unlocker xkr-pool-payments xkr-pool-charts
```

The pool runs as separate modules (`init.js -module=pool|api|unlocker|payments|chartsDataCollector`), each in its own unit. Until
payouts are proven, restrict the stratum ports with `poolServer.allowIPs`.

## 6. Website

Copy `website_example/` to the web root, set `poolHost`, the contact and links in `config.js`, and proxy `/api` to the pool API on
127.0.0.1:8145 (`deployment/apache-vhost.conf.example` exposes only the read-only methods).

## 7. Rehearse the payments

1. `payments.dryRun: true`: the log of `xkr-pool-payments` shows what would be paid.
2. Fund the pool wallet with a few coins (or wait for the first block), credit a small balance in Redis to your own test addresses
   (`<coin>:workers:<address>`, field `balance`), set `payments.onlyAccounts` to them, `dryRun: false`, and watch the payout confirm.
3. Remove the test accounts from Redis and set `onlyAccounts` to `[]`.

Good to know: the real coinbase unlock window is 20 blocks (`blockUnlocker.depth` is set higher to be safe); `sendTransaction`'s
`fee`/`anonymity` fields have no automatic default the way a Monero-style wallet infers them (see `payments.sendFee`/`sendAnonymity`
in config.json); `deployment/pause-payments.sh` stops new payouts at once; a wallet cannot abandon a transaction, so a payout that
stays unconfirmed for `pendingTimeoutHours` is only reported in the log and must be checked by hand.
