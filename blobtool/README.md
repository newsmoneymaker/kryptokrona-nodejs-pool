# blobtool

Builds the real, correct PoW-hashing blob for a Kryptokrona block template, exactly the way the node's own
reference miner does it (`src/Miner/MinerManager.cpp`'s `adjustBlockTemplate()` -> `adjustMergeMiningTag()`,
then `src/Miner/Miner.cpp`'s `CachedBlock(block).getBlockLongHash()`) -- NOT just slicing bytes out of
`getblocktemplate`'s raw `blocktemplate_blob`, which is missing a required mutation: the coinbase
transaction's `extra` field gets rewritten with a merge-mining tag (even though this coin does not actually
merge-mine anything) BEFORE the merkle root used for hashing is computed. See `blobtool.cpp`'s own header
comment and `kryptokrona-project.md` for the full story.

Linked directly against the real node's own compiled `libCryptoNoteCore.a`/`libSerialization.a`/
`libCommon.a`/`libCrypto.a`/`libLogging.a`/`libSystem.a` (not reimplemented), so there is no risk of a
hand-rolled parser disagreeing with consensus.

## Build

Needs a checkout of the node's own source tree, already built (`cmake .. && make`), since this links
against its compiled static libraries and uses its headers directly.

```
g++ -std=c++17 -O2 -DRAPIDJSON_HAS_STDSTRING=1 \
  -I<kryptokrona>/include -I<kryptokrona>/build/version -I<kryptokrona>/src \
  -I<kryptokrona>/external -I<kryptokrona>/external/nlohmann-json \
  -I<kryptokrona>/src/Platform/Linux -I<kryptokrona>/src/Platform/Posix \
  -c blobtool.cpp -o blobtool.o

g++ -O2 blobtool.o \
  <kryptokrona>/build/src/libCryptoNoteCore.a <kryptokrona>/build/src/libSerialization.a \
  <kryptokrona>/build/src/libCommon.a <kryptokrona>/build/src/libCrypto.a \
  <kryptokrona>/build/src/libLogging.a <kryptokrona>/build/src/libSystem.a \
  -lboost_system -lboost_filesystem -lboost_thread -lboost_serialization -lpthread -lrt -ldl \
  -o blobtool
```

## Usage

```
./blobtool <blocktemplate_blob hex>
```

Prints 4 lines: `TEMPLATE <hex>` (the merge-mining-tag-adjusted full block, submit THIS with the nonce
spliced in, not the original `blocktemplate_blob`), `HASHING <hex>` (the exact bytes
`cn_turtle_lite_slow_hash_v2` hashes), `TEMPLATE_NONCE_OFFSET <n>`, `HASHING_NONCE_OFFSET <n>` (found
empirically by a marker-swap-and-locate technique, not by hand-tracing the serializer -- see the source
comment), and `PREVHASH <hex>` (straight from the parsed struct, since this daemon's `getblocktemplate`
doesn't echo one). `lib/blobtool.js` in this repo is the Node.js wrapper that calls this binary over SSH.
