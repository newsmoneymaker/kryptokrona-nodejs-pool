/**
 * Kryptokrona (XKR) Pool (based on cryptonote-nodejs-pool, GPL-2.0)
 *
 * Wrapper around the real node's own block-template adjustment (blobtool, linked against the node's
 * own compiled libCryptoNoteCore.a etc -- see kryptokrona-project.md): the node's reference miner
 * ALWAYS rewrites the coinbase transaction's `extra` field with a merge-mining tag before computing the
 * PoW hash (src/Miner/MinerManager.cpp's adjustMergeMiningTag(), see blobtool.cpp's own header comment)
 * -- skipping this produces a hashing blob the real network would never accept. Hand-rolling this in JS
 * (parsing the raw blocktemplate_blob, re-deriving the merkle root, the merge-mining tag bytes, etc) was
 * judged too risky to get right without real consensus code backing it, so the pool asks the node's own
 * code (via SSH to srv3, where the node is built) to do it, once per new block template (not per share).
 *
 * run(blobHex, cb)  cb(err, {template, hashing, templateNonceOffset, hashingNonceOffset})
 **/
let execFile = require('child_process').execFile;

let toolConfig = config.blobtool || {};
let sshHost = toolConfig.sshHost || 'srv3';
let remotePath = toolConfig.remotePath || '/root/kryptokrona/blobtool';

exports.run = function (blobHex, callback) {
	execFile('ssh', [sshHost, remotePath, blobHex], {timeout: 10000, maxBuffer: 1024 * 1024}, function (err, stdout, stderr) {
		if (err) return callback(new Error('blobtool failed: ' + (stderr || err.message).trim()));

		let result = {};
		stdout.split('\n').forEach(function (line) {
			let parts = line.trim().split(' ');
			if (parts[0] === 'TEMPLATE') result.template = parts[1];
			else if (parts[0] === 'HASHING') result.hashing = parts[1];
			else if (parts[0] === 'TEMPLATE_NONCE_OFFSET') result.templateNonceOffset = parseInt(parts[1], 10);
			else if (parts[0] === 'HASHING_NONCE_OFFSET') result.hashingNonceOffset = parseInt(parts[1], 10);
			else if (parts[0] === 'PREVHASH') result.prevHash = parts[1];
		});

		if (!result.template || !result.hashing || !(result.templateNonceOffset >= 0) || !(result.hashingNonceOffset >= 0)) {
			return callback(new Error('blobtool gave an incomplete answer: ' + stdout.trim()));
		}

		callback(null, result);
	});
};
