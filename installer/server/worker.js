// ASCENT content server: a Cloudflare Worker in front of the private R2
// bucket "ascent-content" (the 1.3 patch pack and the upscaled packs).
//
// Bindings (set at deploy time, see installer/server/README.md):
//   BUCKET        R2 bucket ascent-content
//   CLIENT_TOKEN  the value of client_token in installer/content.ini
//   LIMITER       rate limit binding (optional)
//
// The installer and ascent-content send the header X-Ascent-Client. The
// check is a deterrent, not a protection: anyone can copy the header from
// the public client. Requests without it get 404, as if nothing is here.
//
// GET and HEAD only. Range requests are supported, so that a download can
// continue after a break.

const HEADER = 'X-Ascent-Client';

function notFound() {
	return new Response('Not found', { status: 404 });
}

export default {
	async fetch(request, env) {
		if (request.method !== 'GET' && request.method !== 'HEAD')
			return new Response('Method not allowed', { status: 405, headers: { Allow: 'GET, HEAD' } });

		if (!env.CLIENT_TOKEN || request.headers.get(HEADER) !== env.CLIENT_TOKEN)
			return notFound();

		if (env.LIMITER) {
			const ip = request.headers.get('CF-Connecting-IP') || 'unknown';
			const { success } = await env.LIMITER.limit({ key: ip });
			if (!success)
				return new Response('Too many requests', { status: 429, headers: { 'Retry-After': '60' } });
		}

		const url = new URL(request.url);
		const key = decodeURIComponent(url.pathname.replace(/^\/+/, ''));
		if (!key || key.includes('..') || key.endsWith('/'))
			return notFound();

		if (request.method === 'HEAD') {
			const head = await env.BUCKET.head(key);
			if (!head)
				return notFound();
			const h = new Headers();
			head.writeHttpMetadata(h);
			h.set('ETag', head.httpEtag);
			h.set('Content-Length', String(head.size));
			h.set('Accept-Ranges', 'bytes');
			return new Response(null, { status: 200, headers: h });
		}

		const obj = await env.BUCKET.get(key, { range: request.headers, onlyIf: request.headers });
		if (!obj)
			return notFound();

		const h = new Headers();
		obj.writeHttpMetadata(h);
		h.set('ETag', obj.httpEtag);
		h.set('Accept-Ranges', 'bytes');
		h.set('Cache-Control', 'private, no-store');

		// onlyIf did not match: R2 returns the object without a body.
		if (!('body' in obj) || obj.body === null)
			return new Response(null, { status: 304, headers: h });

		if (obj.range && request.headers.has('Range')) {
			const r = obj.range;
			const offset = r.offset !== undefined ? r.offset : obj.size - r.suffix;
			const length = r.length !== undefined ? r.length : obj.size - offset;
			h.set('Content-Range', `bytes ${offset}-${offset + length - 1}/${obj.size}`);
			h.set('Content-Length', String(length));
			return new Response(obj.body, { status: 206, headers: h });
		}

		h.set('Content-Length', String(obj.size));
		return new Response(obj.body, { status: 200, headers: h });
	},
};
