/**
 * Kryptokrona (XKR) Pool (based on cryptonote-nodejs-pool, GPL-2.0)
 *
 * JSON-RPC 2.0 client of the Kryptokrona daemon (kryptokronad, /json_rpc) and of the pool wallet
 * (kryptokrona-service). Settings: config.node {host, port} (default 127.0.0.1:11898, reached through
 * the xkr-tunnel SSH forward to srv3 -- see kryptokrona-project.md, the real daemon RPC has no auth and
 * is not exposed on any public interface), config.wallet {host, port}.
 **/
let http = require('http');

/**
 * call(method, params, callback(err, result), options). err is {code, message} for an RPC error (the other side said no),
 * an Error for a transport problem (outcome unknown). options: {wallet: true} talks to the wallet, timeout in ms.
 **/
exports.call = function (method, params, callback, options) {
	options = options || {};
	let target = (options.wallet ? config.wallet : config.node) || {};
	let body = JSON.stringify({jsonrpc: '2.0', id: '0', method: method, params: params || {}});
	let done = false;
	let finish = function (err, result) {
		if (done) return;
		done = true;
		callback(err, result);
	};

	let req = http.request({
		host: target.host || '127.0.0.1',
		port: target.port || (options.wallet ? 8198 : 8197),
		path: '/json_rpc',
		method: 'POST',
		headers: {'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(body)},
		timeout: options.timeout || 30000
	}, function (res) {
		let chunks = [];
		res.on('data', function (c) { chunks.push(c); });
		res.on('end', function () {
			let text = Buffer.concat(chunks).toString('utf8');
			let parsed;
			try {
				parsed = JSON.parse(text);
			} catch (e) {
				return finish(new Error('Bad answer from the ' + (options.wallet ? 'wallet' : 'daemon') + ' (HTTP ' + res.statusCode + '): ' + text.slice(0, 120)));
			}
			if (parsed.error) return finish(parsed.error);
			finish(null, parsed.result);
		});
	});
	req.on('timeout', function () { req.destroy(new Error('RPC timeout: ' + method)); });
	req.on('error', function (e) { finish(e); });
	req.end(body);
};
