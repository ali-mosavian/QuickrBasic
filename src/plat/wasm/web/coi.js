// Cross-origin isolation where the server sends no COOP/COEP headers (GitHub Pages): this service
// worker adds them to every response, and the page reloads once under it. SharedArrayBuffer needs it.
if (typeof window === 'undefined') {
  self.addEventListener('install', () => self.skipWaiting());
  self.addEventListener('activate', (e) => e.waitUntil(self.clients.claim()));
  self.addEventListener('fetch', (e) => {
    if (e.request.cache === 'only-if-cached' && e.request.mode !== 'same-origin') return;
    e.respondWith(fetch(e.request).then((r) => {
      if (r.status === 0) return r;
      const headers = new Headers(r.headers);
      headers.set('Cross-Origin-Opener-Policy', 'same-origin');
      headers.set('Cross-Origin-Embedder-Policy', 'require-corp');
      headers.set('Cross-Origin-Resource-Policy', 'same-origin');
      return new Response(r.body, { status: r.status, statusText: r.statusText, headers });
    }));
  });
} else if (!crossOriginIsolated && 'serviceWorker' in navigator) {
  navigator.serviceWorker.register(document.currentScript.src).then((reg) => {
    if (reg.active && !navigator.serviceWorker.controller) location.reload();
    else reg.addEventListener('updatefound', () => reg.installing.addEventListener('statechange', (e) => {
      if (e.target.state === 'activated') location.reload();
    }));
  });
}
