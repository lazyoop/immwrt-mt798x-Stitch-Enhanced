if(!self.define){const e=e=>{"require"!==e&&(e+=".js");let s=Promise.resolve();return r[e]||(s=new Promise(async s=>{if("document"in self){const r=document.createElement("script");r.src=e,document.head.appendChild(r),r.onload=s}else importScripts(e),s()})),s.then(()=>{if(!r[e])throw new Error(`Module ${e} didn’t register its module`);return r[e]})},s=(s,r)=>{Promise.all(s.map(e)).then(e=>r(1===e.length?e[0]:e))},r={require:Promise.resolve(s)};self.define=(s,i,t)=>{r[s]||(r[s]=Promise.resolve().then(()=>{let r={};const c={uri:location.origin+s.slice(1)};return Promise.all(i.map(s=>{switch(s){case"exports":return r;case"module":return c;default:return e(s)}})).then(e=>{const s=t(...e);return r.default||(r.default=s),r})}))}}define("./service-worker.js",["./workbox-8a532145"],(function(e){"use strict";self.addEventListener("message",e=>{e.data&&"SKIP_WAITING"===e.data.type&&self.skipWaiting()}),e.clientsClaim(),e.precacheAndRoute([{url:"./index.html",revision:"ff336a0edf18fc90c7f03fb0e43250a0"},{url:"./static/css/main.83e4b341.chunk.css",revision:"5a48466e5ff2a8ceec3348c3c498c260"},{url:"./static/js/2.6366a4a0.chunk.js",revision:"7a8970333d728cf369978e1fa658787e"},{url:"./static/js/2.6366a4a0.chunk.js.LICENSE.txt",revision:"6557abd439259b9580e2e2d5633cdacb"},{url:"./static/js/main.3ba71f82.chunk.js",revision:"2c4d6d7f7a1d7c843c445be62f92109f"},{url:"./static/js/runtime-main.fefd5bb9.js",revision:"98f4f314ef9abb714a6c74c41e1e5a04"},{url:"./static/media/logo.45983944.png",revision:"198a55c9efe85ab0145d4402c07cfe65"}],{}),e.registerRoute(new e.NavigationRoute(e.createHandlerBoundToURL("./index.html"),{denylist:[/^\/_/,/\/[^/?]+\.[^/]+$/]}))}));
