define("./workbox-8a532145.js",["exports"],(function(e){"use strict";try{self["workbox:core:5.1.4"]&&_()}catch(e){}const t={googleAnalytics:"googleAnalytics",precache:"precache-v2",prefix:"workbox",runtime:"runtime",suffix:"undefined"!=typeof registration?registration.scope:""},n=e=>[t.prefix,e,t.suffix].filter(e=>e&&e.length>0).join("-"),s=e=>e||n(t.precache),r=e=>new URL(String(e),location.href).href.replace(new RegExp(`^${location.origin}`),""),i=(e,...t)=>{let n=e;return t.length>0&&(n+=` :: ${JSON.stringify(t)}`),n};class o extends Error{constructor(e,t){super(i(e,t)),this.name=e,this.details=t}}const c=new Set;const a=(e,t)=>e.filter(e=>t in e),u=async({request:e,mode:t,plugins:n=[]})=>{const s=a(n,"cacheKeyWillBeUsed");let r=e;for(const e of s)r=await e.cacheKeyWillBeUsed.call(e,{mode:t,request:r}),"string"==typeof r&&(r=new Request(r));return r},h=async({cacheName:e,request:t,event:n,matchOptions:s,plugins:r=[]})=>{const i=await self.caches.open(e),o=await u({plugins:r,request:t,mode:"read"});let c=await i.match(o,s);for(const t of r)if("cachedResponseWillBeUsed"in t){const r=t.cachedResponseWillBeUsed;c=await r.call(t,{cacheName:e,event:n,matchOptions:s,cachedResponse:c,request:o})}return c},l=async({cacheName:e,request:t,response:n,event:s,plugins:i=[],matchOptions:l})=>{const f=await u({plugins:i,request:t,mode:"write"});if(!n)throw new o("cache-put-with-no-response",{url:r(f.url)});const w=await(async({request:e,response:t,event:n,plugins:s=[]})=>{let r=t,i=!1;for(const t of s)if("cacheWillUpdate"in t){i=!0;const s=t.cacheWillUpdate;if(r=await s.call(t,{request:e,response:r,event:n}),!r)break}return i||(r=r&&200===r.status?r:void 0),r||null})({event:s,plugins:i,response:n,request:f});if(!w)return;const d=await self.caches.open(e),p=a(i,"cacheDidUpdate"),y=p.length>0?await h({cacheName:e,matchOptions:l,request:f}):null;try{await d.put(f,w)}catch(e){throw"QuotaExceededError"===e.name&&await async function(){for(const e of c)await e()}(),e}for(const t of p)await t.cacheDidUpdate.call(t,{cacheName:e,event:s,oldResponse:y,newResponse:w,request:f})},f=async({request:e,fetchOptions:t,event:n,plugins:s=[]})=>{if("string"==typeof e&&(e=new Request(e)),n instanceof FetchEvent&&n.preloadResponse){const e=await n.preloadResponse;if(e)return e}const r=a(s,"fetchDidFail"),i=r.length>0?e.clone():null;try{for(const t of s)if("requestWillFetch"in t){const s=t.requestWillFetch,r=e.clone();e=await s.call(t,{request:r,event:n})}}catch(e){throw new o("plugin-error-request-will-fetch",{thrownError:e})}const c=e.clone();try{let r;r="navigate"===e.mode?await fetch(e):await fetch(e,t);for(const e of s)"fetchDidSucceed"in e&&(r=await e.fetchDidSucceed.call(e,{event:n,request:c,response:r}));return r}catch(e){for(const t of r)await t.fetchDidFail.call(t,{error:e,event:n,originalRequest:i.clone(),request:c.clone()});throw e}};let w;async function d(e,t){const n=e.clone(),s={headers:new Headers(n.headers),status:n.status,statusText:n.statusText},r=t?t(s):s,i=function(){if(void 0===w){const e=new Response("");if("body"in e)try{new Response(e.body),w=!0}catch(e){w=!1}w=!1}return w}()?n.body:await n.blob();return new Response(i,r)}try{self["workbox:precaching:5.1.4"]&&_()}catch(e){}function p(e){if(!e)throw new o("add-to-cache-list-unexpected-type",{entry:e});if("string"==typeof e){const t=new URL(e,location.href);return{cacheKey:t.href,url:t.href}}const{revision:t,url:n}=e;if(!n)throw new o("add-to-cache-list-unexpected-type",{entry:e});if(!t){const e=new URL(n,location.href);return{cacheKey:e.href,url:e.href}}const s=new URL(n,location.href),r=new URL(n,location.href);return s.searchParams.set("__WB_REVISION__",t),{cacheKey:s.href,url:r.href}}class y{constructor(e){this.t=s(e),this.s=new Map,this.i=new Map,this.o=new Map}addToCacheList(e){const t=[];for(const n of e){"string"==typeof n?t.push(n):n&&void 0===n.revision&&t.push(n.url);const{cacheKey:e,url:s}=p(n),r="string"!=typeof n&&n.revision?"reload":"default";if(this.s.has(s)&&this.s.get(s)!==e)throw new o("add-to-cache-list-conflicting-entries",{firstEntry:this.s.get(s),secondEntry:e});if("string"!=typeof n&&n.integrity){if(this.o.has(e)&&this.o.get(e)!==n.integrity)throw new o("add-to-cache-list-conflicting-integrities",{url:s});this.o.set(e,n.integrity)}if(this.s.set(s,e),this.i.set(s,r),t.length>0){const e="Workbox is precaching URLs without revision "+`info: ${t.join(", ")}\nThis is generally NOT safe. `+"Learn more at https://bit.ly/wb-precache";console.warn(e)}}}async install({event:e,plugins:t}={}){const n=[],s=[],r=await self.caches.open(this.t),i=await r.keys(),o=new Set(i.map(e=>e.url));for(const[e,t]of this.s)o.has(t)?s.push(e):n.push({cacheKey:t,url:e});const c=n.map(({cacheKey:n,url:s})=>{const r=this.o.get(n),i=this.i.get(s);return this.u({cacheKey:n,cacheMode:i,event:e,integrity:r,plugins:t,url:s})});return await Promise.all(c),{updatedURLs:n.map(e=>e.url),notUpdatedURLs:s}}async activate(){const e=await self.caches.open(this.t),t=await e.keys(),n=new Set(this.s.values()),s=[];for(const r of t)n.has(r.url)||(await e.delete(r),s.push(r.url));return{deletedURLs:s}}async u({cacheKey:e,url:t,cacheMode:n,event:s,plugins:r,integrity:i}){const c=new Request(t,{integrity:i,cache:n,credentials:"same-origin"});let a,u=await f({event:s,plugins:r,request:c});for(const e of r||[])"cacheWillUpdate"in e&&(a=e);if(!(a?await a.cacheWillUpdate({event:s,request:c,response:u}):u.status<400))throw new o("bad-precaching-response",{url:t,status:u.status});u.redirected&&(u=await d(u)),await l({event:s,plugins:r,response:u,request:e===t?c:new Request(e),cacheName:this.t,matchOptions:{ignoreSearch:!0}})}getURLsToCacheKeys(){return this.s}getCachedURLs(){return[...this.s.keys()]}getCacheKeyForURL(e){const t=new URL(e,location.href);return this.s.get(t.href)}async matchPrecache(e){const t=e instanceof Request?e.url:e,n=this.getCacheKeyForURL(t);if(n){return(await self.caches.open(this.t)).match(n)}}createHandler(e=!0){return async({request:t})=>{try{const e=await this.matchPrecache(t);if(e)return e;throw new o("missing-precache-entry",{cacheName:this.t,url:t instanceof Request?t.url:t})}catch(n){if(e)return fetch(t);throw n}}}createHandlerBoundToURL(e,t=!0){if(!this.getCacheKeyForURL(e))throw new o("non-precached-url",{url:e});const n=this.createHandler(t),s=new Request(e);return()=>n({request:s})}}let g;const R=()=>(g||(g=new y),g);const m=(e,t)=>{const n=R().getURLsToCacheKeys();for(const s of function*(e,{ignoreURLParametersMatching:t,directoryIndex:n,cleanURLs:s,urlManipulation:r}={}){const i=new URL(e,location.href);i.hash="",yield i.href;const o=function(e,t=[]){for(const n of[...e.searchParams.keys()])t.some(e=>e.test(n))&&e.searchParams.delete(n);return e}(i,t);if(yield o.href,n&&o.pathname.endsWith("/")){const e=new URL(o.href);e.pathname+=n,yield e.href}if(s){const e=new URL(o.href);e.pathname+=".html",yield e.href}if(r){const e=r({url:i});for(const t of e)yield t.href}}(e,t)){const e=n.get(s);if(e)return e}};let q=!1;function v(e){q||((({ignoreURLParametersMatching:e=[/^utm_/],directoryIndex:t="index.html",cleanURLs:n=!0,urlManipulation:r}={})=>{const i=s();self.addEventListener("fetch",s=>{const o=m(s.request.url,{cleanURLs:n,directoryIndex:t,ignoreURLParametersMatching:e,urlManipulation:r});if(!o)return;let c=self.caches.open(i).then(e=>e.match(o)).then(e=>e||fetch(o));s.respondWith(c)})})(e),q=!0)}const U=[],L={get:()=>U,add(e){U.push(...e)}},x=e=>{const t=R(),n=L.get();e.waitUntil(t.install({event:e,plugins:n}).catch(e=>{throw e}))},b=e=>{const t=R();e.waitUntil(t.activate())};try{self["workbox:routing:5.1.4"]&&_()}catch(e){}const M=e=>e&&"object"==typeof e?e:{handle:e};class N{constructor(e,t,n="GET"){this.handler=M(t),this.match=e,this.method=n}}class E extends N{constructor(e,t,n){super(({url:t})=>{const n=e.exec(t.href);if(n&&(t.origin===location.origin||0===n.index))return n.slice(1)},t,n)}}class K{constructor(){this.h=new Map}get routes(){return this.h}addFetchListener(){self.addEventListener("fetch",e=>{const{request:t}=e,n=this.handleRequest({request:t,event:e});n&&e.respondWith(n)})}addCacheListener(){self.addEventListener("message",e=>{if(e.data&&"CACHE_URLS"===e.data.type){const{payload:t}=e.data,n=Promise.all(t.urlsToCache.map(e=>{"string"==typeof e&&(e=[e]);const t=new Request(...e);return this.handleRequest({request:t})}));e.waitUntil(n),e.ports&&e.ports[0]&&n.then(()=>e.ports[0].postMessage(!0))}})}handleRequest({request:e,event:t}){const n=new URL(e.url,location.href);if(!n.protocol.startsWith("http"))return;const{params:s,route:r}=this.findMatchingRoute({url:n,request:e,event:t});let i,o=r&&r.handler;if(!o&&this.l&&(o=this.l),o){try{i=o.handle({url:n,request:e,event:t,params:s})}catch(e){i=Promise.reject(e)}return i instanceof Promise&&this.p&&(i=i.catch(s=>this.p.handle({url:n,request:e,event:t}))),i}}findMatchingRoute({url:e,request:t,event:n}){const s=this.h.get(t.method)||[];for(const r of s){let s;const i=r.match({url:e,request:t,event:n});if(i)return s=i,(Array.isArray(i)&&0===i.length||i.constructor===Object&&0===Object.keys(i).length||"boolean"==typeof i)&&(s=void 0),{route:r,params:s}}return{}}setDefaultHandler(e){this.l=M(e)}setCatchHandler(e){this.p=M(e)}registerRoute(e){this.h.has(e.method)||this.h.set(e.method,[]),this.h.get(e.method).push(e)}unregisterRoute(e){if(!this.h.has(e.method))throw new o("unregister-route-but-not-found-with-method",{method:e.method});const t=this.h.get(e.method).indexOf(e);if(!(t>-1))throw new o("unregister-route-route-not-registered");this.h.get(e.method).splice(t,1)}}let O;const C=()=>(O||(O=new K,O.addFetchListener(),O.addCacheListener()),O);e.NavigationRoute=class extends N{constructor(e,{allowlist:t=[/./],denylist:n=[]}={}){super(e=>this.g(e),e),this.R=t,this.m=n}g({url:e,request:t}){if(t&&"navigate"!==t.mode)return!1;const n=e.pathname+e.search;for(const e of this.m)if(e.test(n))return!1;return!!this.R.some(e=>e.test(n))}},e.clientsClaim=function(){self.addEventListener("activate",()=>self.clients.claim())},e.createHandlerBoundToURL=function(e){return R().createHandlerBoundToURL(e)},e.precacheAndRoute=function(e,t){!function(e){R().addToCacheList(e),e.length>0&&(self.addEventListener("install",x),self.addEventListener("activate",b))}(e),v(t)},e.registerRoute=function(e,t,n){let s;if("string"==typeof e){const r=new URL(e,location.href);s=new N(({url:e})=>e.href===r.href,t,n)}else if(e instanceof RegExp)s=new E(e,t,n);else if("function"==typeof e)s=new N(e,t,n);else{if(!(e instanceof N))throw new o("unsupported-route-type",{moduleName:"workbox-routing",funcName:"registerRoute",paramName:"capture"});s=e}return C().registerRoute(s),s}}));
