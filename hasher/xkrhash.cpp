// xkrhash: share validation helper of the Kryptokrona (XKR) pool.
// cn_turtle_lite_slow_hash_v2 sources (xkr-src/) are vendored verbatim from Kryptokrona's OWN node
// source (src/crypto/ on srv3, 213.239.211.209), with every symbol renamed xkr_ to avoid colliding with
// another vendored coin's crypto -- see critical-vendored-lib-symbol-collision and
// kryptokrona-project.md. Deliberately does NOT reuse poolpayminer's own Algorithm::CN_PICO_0
// ("cn-pico"/"cryptonight-turtle") -- cross-checked against this exact real node code and confirmed
// CN_PICO_0 produces a DIFFERENT hash for the same input despite matching memory/iteration parameters,
// see critical-wrong-randomx-variant-bug.
//
// Reads commands on stdin, writes answers on stdout (one line each):
//   H <id> <hex>   the exact blob bytes the real node's cn_turtle_lite_slow_hash_v2 hashes (the pool's
//                  own blockTemplate blob with the nonce spliced in, see blockBuilder.js); answers
//                  "H <id> <64 hex>" (32 byte hash) or "H <id> err"
//   Q              quit
// Command line: --hash <hex> prints the hash.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>

extern "C" void xkr_cn_slow_hash(const void *data, size_t length, char *hash,
                                  int light, int variant, int prehashed,
                                  uint32_t page_size, uint32_t scratchpad, uint32_t iterations);

static int hexval(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static int from_hex(const char *s, std::vector<unsigned char> &out)
{
	size_t n = strlen(s);
	if (n % 2) return -1;
	out.resize(n / 2);
	for (size_t i = 0; i < out.size(); i++) {
		int a = hexval(s[2 * i]), b = hexval(s[2 * i + 1]);
		if (a < 0 || b < 0) return -1;
		out[i] = (unsigned char)(a * 16 + b);
	}
	return 0;
}

static void to_hex(const unsigned char *in, size_t n, char *out)
{
	static const char d[] = "0123456789abcdef";
	for (size_t i = 0; i < n; i++) { out[2 * i] = d[in[i] >> 4]; out[2 * i + 1] = d[in[i] & 15]; }
	out[2 * n] = 0;
}

static void xkrHash(const std::vector<unsigned char> &data, unsigned char hash[32])
{
	// light=1, variant=2, prehashed=0, page_size=scratchpad=262144, iterations=131072 -- exactly
	// cn_turtle_lite_slow_hash_v2 in the real node's own src/crypto/hash.h.
	xkr_cn_slow_hash(data.data(), data.size(), reinterpret_cast<char *>(hash), 1, 2, 0, 262144, 262144, 131072);
}

int main(int argc, char **argv)
{
	unsigned char hash[32];
	char hex[65];
	std::vector<unsigned char> data;

	if (argc == 3 && strcmp(argv[1], "--hash") == 0) {
		if (from_hex(argv[2], data)) { fprintf(stderr, "bad hex\n"); return 1; }
		xkrHash(data, hash);
		to_hex(hash, 32, hex);
		puts(hex);
		return 0;
	}

	char line[8192];
	while (fgets(line, sizeof(line), stdin)) {
		if (line[0] == 'Q') break;
		if (line[0] != 'H') continue;
		char id[64], hexdata[8000];
		if (sscanf(line, "H %63s %7999s", id, hexdata) != 2 || from_hex(hexdata, data)) {
			printf("H %s err\n", (strlen(line) > 2) ? id : "0");
			fflush(stdout);
			continue;
		}
		xkrHash(data, hash);
		to_hex(hash, 32, hex);
		printf("H %s %s\n", id, hex);
		fflush(stdout);
	}
	return 0;
}
