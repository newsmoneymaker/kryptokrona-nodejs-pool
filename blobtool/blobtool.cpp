// blobtool: builds the REAL, correct PoW-hashing blob for a Kryptokrona block template, exactly the way
// the node's own reference miner does it (src/Miner/MinerManager.cpp's adjustBlockTemplate() ->
// adjustMergeMiningTag(), then src/Miner/Miner.cpp's CachedBlock(block).getBlockLongHash()) -- NOT just
// slicing bytes out of getblocktemplate's raw blocktemplate_blob, which is missing a required mutation:
// the coinbase transaction's `extra` field gets rewritten with a merge-mining tag (even though this coin
// does not actually merge-mine anything) BEFORE the merkle root used for hashing is computed. Skipping
// this produces a hashing blob the real network would never accept. See kryptokrona-project.md.
//
// Linked directly against the real node's own compiled libCryptoNoteCore.a/libSerialization.a/etc
// (not reimplemented) so there is no risk of a hand-rolled parser disagreeing with consensus.
//
// Usage: blobtool <blocktemplate_blob hex>
// Prints 3 lines:
//   TEMPLATE <hex>      the merge-mining-tag-adjusted FULL block (same shape as blocktemplate_blob,
//                       nonce field spliceable at NONCE_OFFSET below) -- submit THIS (with nonce+extra
//                       nonce spliced in), not the original blocktemplate_blob.
//   HASHING <hex>       the exact bytes cn_turtle_lite_slow_hash_v2 hashes (nonce spliceable at the
//                       SAME NONCE_OFFSET -- the hashing blob and the adjusted template share the same
//                       prefix up to and including the nonce field).
//   NONCE_OFFSET <n>    byte offset of the 4 byte nonce in BOTH of the above.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "CryptoNoteCore/CachedBlock.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/TransactionExtra.h"
#include "config/CryptoNoteConfig.h"
#include "Common/StringTools.h"

using namespace CryptoNote;

static void adjustMergeMiningTag(BlockTemplate& blockTemplate)
{
    CachedBlock cachedBlock(blockTemplate);

    if (blockTemplate.majorVersion >= BLOCK_MAJOR_VERSION_2)
    {
        CryptoNote::TransactionExtraMergeMiningTag mmTag;
        mmTag.depth = 0;
        mmTag.merkleRoot = cachedBlock.getAuxiliaryBlockHeaderHash();

        blockTemplate.parentBlock.baseTransaction.extra.clear();
        if (!CryptoNote::appendMergeMiningTagToExtra(blockTemplate.parentBlock.baseTransaction.extra, mmTag))
        {
            throw std::runtime_error("Couldn't append merge mining tag");
        }
    }
}

static void toHexOut(const char *label, const BinaryArray &data)
{
    printf("%s %s\n", label, Common::toHex(data).c_str());
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <blocktemplate_blob hex>\n", argv[0]);
        return 1;
    }

    std::vector<uint8_t> blob = Common::fromHex(std::string(argv[1]));

    BlockTemplate blockTemplate;
    if (!fromBinaryArray(blockTemplate, blob))
    {
        fprintf(stderr, "could not parse block template\n");
        return 1;
    }

    adjustMergeMiningTag(blockTemplate);

    BinaryArray adjustedTemplate;
    if (!toBinaryArray(blockTemplate, adjustedTemplate))
    {
        fprintf(stderr, "could not re-serialize adjusted template\n");
        return 1;
    }

    auto buildHashingBlob = [](BlockTemplate &bt) {
        CachedBlock cb(bt);
        if (bt.majorVersion == BLOCK_MAJOR_VERSION_1) {
            return cb.getBlockHashingBinaryArray();
        }
        return cb.getParentBlockHashingBinaryArray(true);
    };

    BinaryArray hashingBlob = buildHashingBlob(blockTemplate);

    // Nonce offset: found empirically, NOT by hand-tracing the serializer's field order (BlockTemplate's
    // own top-level header and the "parent_block" sub-structure used for hashing have genuinely
    // different, independently-ordered serializers -- see CryptoNoteSerialization.cpp's
    // `serialize(BlockTemplate&, ...)` vs `serialize(ParentBlockSerializer&, ...)`; tracing both by eye
    // and trusting the derived offset would be exactly the kind of subtle protocol mistake
    // critical-getblocktemplate-incomplete-block warns about). Instead: set the real nonce to a
    // recognisable marker, rebuild both arrays, and locate the marker's unique byte position directly.
    uint32_t savedNonce = blockTemplate.nonce;
    blockTemplate.nonce = 0xDEADBEEFu;
    BinaryArray markedTemplate;
    if (!toBinaryArray(blockTemplate, markedTemplate)) { fprintf(stderr, "re-serialize (marked) failed\n"); return 1; }
    BinaryArray markedHashing = buildHashingBlob(blockTemplate);
    blockTemplate.nonce = savedNonce;

    static const uint8_t marker[4] = {0xEF, 0xBE, 0xAD, 0xDE}; // 0xDEADBEEF, little endian

    auto findOffset = [&](const BinaryArray &marked, const BinaryArray &original, const char *what) -> long {
        std::vector<size_t> hits;
        for (size_t i = 0; i + 4 <= marked.size(); i++) {
            if (memcmp(&marked[i], marker, 4) == 0) hits.push_back(i);
        }
        if (hits.size() != 1) {
            fprintf(stderr, "%s: marker found %zu times (expected exactly 1), cannot determine nonce offset safely\n", what, hits.size());
            return -1;
        }
        return static_cast<long>(hits[0]);
    };

    long templateOffset = findOffset(markedTemplate, adjustedTemplate, "TEMPLATE");
    long hashingOffset = findOffset(markedHashing, hashingBlob, "HASHING");
    if (templateOffset < 0 || hashingOffset < 0) return 1;

    // Sanity: the marked arrays, with the marker zeroed back out, must equal the originals exactly --
    // proves the marker swap didn't perturb anything else (e.g. a varint whose length depends on the
    // nonce's own value).
    BinaryArray check = markedTemplate;
    memcpy(&check[templateOffset], &adjustedTemplate[templateOffset], 4);
    if (check != adjustedTemplate) { fprintf(stderr, "TEMPLATE: marker swap perturbed other bytes (varint length changed?) -- unsafe\n"); return 1; }
    check = markedHashing;
    memcpy(&check[hashingOffset], &hashingBlob[hashingOffset], 4);
    if (check != hashingBlob) { fprintf(stderr, "HASHING: marker swap perturbed other bytes (varint length changed?) -- unsafe\n"); return 1; }

    toHexOut("TEMPLATE", adjustedTemplate);
    toHexOut("HASHING", hashingBlob);
    printf("TEMPLATE_NONCE_OFFSET %ld\n", templateOffset);
    printf("HASHING_NONCE_OFFSET %ld\n", hashingOffset);
    // The real previousBlockHash straight from the daemon's own parsed struct -- this daemon's
    // getblocktemplate RPC does not echo a prev_hash field, unlike more modern CryptoNote forks.
    printf("PREVHASH %s\n", Common::podToHex(blockTemplate.previousBlockHash).c_str());

    return 0;
}
