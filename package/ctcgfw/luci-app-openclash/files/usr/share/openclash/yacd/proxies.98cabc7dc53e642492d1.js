(window.webpackJsonp=window.webpackJsonp||[]).push([[2],{"5JRb":function(e,t,a){e.exports={labeledInput:"_1h4Qixiwxj"}},D46e:function(e,t,a){e.exports={header:"_3PCSxT0l14",list:"_1yYRIyvlRd",proxy:"_1OcDlvlM5R",proxySelectable:"_3oAxPKtZFv"}},FWZb:function(e,t,a){e.exports={proxy:"NpfXwxWAxo",now:"_2zD7drviYH",error:"_2bylJNYYdN",proxyType:"_1rVl-Kdmss",row:"aS2noEcBcP",proxyName:"_3kdi5nima5",proxySmall:"_3NpxWygDxO"}},L5YO:function(e,t,a){e.exports={topBar:"_39uWX_-wQq",group:"_1myfcMimT9"}},RL2M:function(e,t,a){e.exports={header:"_37wt2_a2Gx",arrow:"L80zPM0Rx0",isOpen:"_36RO7_wtuv",btn:"_2XKN3NArpV",qty:"_1s98xvUoBx"}},RbL9:function(e,t,a){e.exports={proxyLatency:"_2V-RqIAl7n"}},UVHL:function(e,t,a){e.exports={select:"t6lsDKNXTK"}},UmKA:function(e,t,a){e.exports={overlay:"_2OKIZuCJtW",cnt:"_1y6NeshM4O",afterOpen:"_25KARE4UsT"}},wyCT:function(e,t,a){"use strict";a.r(t);a("2B1R");var n=a("ODXe"),r=a("q1tI"),c=a.n(r),l=a("WfPo"),o=a("n57c"),i=a("DKqX"),s=(a("TeQF"),a("ToJy"),a("sMBO"),a("KQm4")),u=a("rePB"),m=a("iuhU"),p=a("Wwog"),v=a("HGr0"),b=a("OADI"),d=a("j7o3"),f=a("riol"),y=a("RL2M"),O=a.n(y);function E(e){var t=e.name,a=e.type,n=e.toggle,r=e.isOpen,l=e.qty;return c.a.createElement("div",{className:O.a.header},c.a.createElement("div",{onClick:n,style:{cursor:"pointer"}},c.a.createElement(f.b,{name:t,type:a})),"number"==typeof l?c.a.createElement("span",{className:O.a.qty},l):null,c.a.createElement(o.a,{kind:"minimal",onClick:n,className:O.a.btn},c.a.createElement("span",{className:Object(m.a)(O.a.arrow,Object(u.a)({},O.a.isOpen,r))},c.a.createElement(d.a,{size:20}))))}var x=a("RbL9"),h=a.n(x);function j(e){var t=e.number,a=e.color;return c.a.createElement("span",{className:h.a.proxyLatency,style:{color:a}},c.a.createElement("span",null,t," ms"))}var g=a("FWZb"),w=a.n(g),N=c.a.useMemo,C="#67c23a",k="#d4b75c",A="#e67f3c",_="#909399";function L(){var e=arguments.length>0&&void 0!==arguments[0]?arguments[0]:{},t=e.number;return t<200?C:t<400?k:"number"==typeof t?A:_}var R=function(e,t){var a=t.name,n=Object(v.c)(e),r=Object(v.b)(e);return{proxy:n[a],latency:r[a]}},S=Object(l.a)(R)((function(e){var t,a=e.now,n=e.name,r=e.proxy,l=e.latency,o=N((function(){return L(l)}),[l]);return c.a.createElement("div",{className:Object(m.a)(w.a.proxy,(t={},Object(u.a)(t,w.a.now,a),Object(u.a)(t,w.a.error,l&&l.error),t))},c.a.createElement("div",{className:w.a.proxyName},n),c.a.createElement("div",{className:w.a.row},c.a.createElement("span",{className:w.a.proxyType,style:{opacity:a?.6:.2}},r.type),l&&l.number?c.a.createElement(j,{number:l.number,color:o}):null))})),D=Object(l.a)(R)((function(e){var t=e.now,a=e.name,n=e.latency,r=N((function(){return L(n)}),[n]),l=N((function(){var e=a;return n&&"number"==typeof n.number&&(e+=" "+n.number+" ms"),e}),[a,n]);return c.a.createElement("div",{title:l,className:Object(m.a)(w.a.proxySmall,Object(u.a)({},w.a.now,t)),style:{backgroundColor:r}})})),P=a("D46e"),B=a.n(P),T=c.a.useCallback,M=c.a.useMemo;function U(e){var t=e.all,a=e.now,n=e.isSelectable,r=e.itemOnTapCallback,l=e.sortedAll||t;return c.a.createElement("div",{className:B.a.list},l.map((function(e){var t=Object(m.a)(B.a.proxy,Object(u.a)({},B.a.proxySelectable,n));return c.a.createElement("div",{className:t,key:e,onClick:function(){n&&r&&r(e)}},c.a.createElement(S,{name:e,now:e===a}))})))}var K=function(e,t){return void 0===e?0:!e.error&&e.number>0?e.number:t};var q={Natural:function(e,t){return e},LatencyAsc:function(e,t){return e.sort((function(e,a){return K(t[e],999999)-K(t[a],999999)}))},LatencyDesc:function(e,t){return e.sort((function(e,a){var n=K(t[e],999999);return K(t[a],999999)-n}))},NameAsc:function(e){return e.sort()},NameDesc:function(e){return e.sort((function(e,t){return e>t?-1:e<t?1:0}))}};var H=Object(p.a)((function(e,t,a,n){var r=Object(s.a)(e);return a&&(r=function(e,t){return e.filter((function(e){var a=t[e];return void 0===a||!a.error&&0!==a.number}))}(e,t)),q[n](r,t)}));function F(e){var t=e.all,a=e.now,n=e.isSelectable,r=e.itemOnTapCallback;return c.a.createElement("div",{className:B.a.list},t.map((function(e){var t=Object(m.a)(B.a.proxy,Object(u.a)({},B.a.proxySelectable,n));return c.a.createElement("div",{className:t,key:e,onClick:function(){n&&r&&r(e)}},c.a.createElement(D,{name:e,now:e===a}))})))}var I=Object(l.a)((function(e,t){var a=t.name,n=t.delay,r=Object(v.c)(e),c=Object(b.c)(e),l=Object(b.f)(e),o=Object(b.d)(e),i=r[a],s=i.all,u=i.type,m=i.now;return{all:H(s,n,o,l),type:u,now:m,isOpen:c["proxyGroup:".concat(a)]}}))((function(e){var t=e.name,a=e.all,n=e.type,r=e.now,o=e.isOpen,i=e.apiConfig,s=e.dispatch,u=M((function(){return"Selector"===n}),[n]),m=Object(l.c)().app.updateCollapsibleIsOpen,p=T((function(){m("proxyGroup",t,!o)}),[o,m,t]),b=T((function(e){u&&s(Object(v.i)(i,t,e))}),[i,s,t,u]);return c.a.createElement("div",{className:B.a.group},c.a.createElement(E,{name:t,type:n,toggle:p,qty:a.length,isOpen:o}),o?c.a.createElement(U,{all:a,now:r,isSelectable:u,itemOnTapCallback:b}):c.a.createElement(F,{all:a}))})),Z=a("9rZX"),z=a.n(Z),W=a("Z9Yo"),V=a.n(W),X=a("UmKA"),Y=a.n(X),J=r.useMemo;function G(e){var t=e.isOpen,a=e.onRequestClose,n=e.children,c=J((function(){return{base:Object(m.a)(V.a.content,Y.a.cnt),afterOpen:Y.a.afterOpen,beforeClose:""}}),[]);return r.createElement(z.a,{isOpen:t,onRequestClose:a,className:c,overlayClassName:Object(m.a)(V.a.overlay,Y.a.overlay)},n)}var Q=a("hkBY"),$=a("mrSG"),ee=a("UVHL"),te=a.n(ee);function ae(e){var t=e.options,a=e.selected,n=e.onChange;return r.createElement("select",{className:te.a.select,value:a,onChange:n},t.map((function(e){var t=Object($.c)(e,2),a=t[0],n=t[1];return r.createElement("option",{key:a,value:a},n)})))}var ne=a("5JRb"),re=a.n(ne),ce=[["Natural","Original order in config file"],["LatencyAsc","By latency from small to big"],["LatencyDesc","By latency from big to small"],["NameAsc","By name alphabetically (A-Z)"],["NameDesc","By name alphabetically (Z-A)"]],le=r.useCallback;var oe=Object(l.a)((function(e){return{appConfig:{proxySortBy:Object(b.f)(e),hideUnavailableProxies:Object(b.d)(e)}}}))((function(e){var t=e.appConfig,a=Object(l.c)().app.updateAppConfig,n=le((function(e){a("proxySortBy",e.target.value)}),[a]),c=le((function(e){a("hideUnavailableProxies",e)}),[a]);return r.createElement(r.Fragment,null,r.createElement("div",{className:re.a.labeledInput},r.createElement("span",null,"Sorting in group"),r.createElement("div",null,r.createElement(ae,{options:ce,selected:t.proxySortBy,onChange:n}))),r.createElement("hr",null),r.createElement("div",{className:re.a.labeledInput},r.createElement("span",null,"Hide unavailable proxies"),r.createElement("div",null,r.createElement(Q.a,{name:"hideUnavailableProxies",checked:t.hideUnavailableProxies,onChange:c}))))}));function ie(e){var t=e.color,a=void 0===t?"currentColor":t,n=e.size,c=void 0===n?24:n;return r.createElement("svg",{fill:"none",xmlns:"http://www.w3.org/2000/svg",viewBox:"0 0 24 24",width:c,height:c,stroke:a,strokeWidth:"2",strokeLinecap:"round",strokeLinejoin:"round"},r.createElement("path",{d:"M2 6h9M18.5 6H22"}),r.createElement("circle",{cx:"16",cy:"6",r:"2"}),r.createElement("path",{d:"M22 18h-9M6 18H2"}),r.createElement("circle",{r:"2",transform:"matrix(-1 0 0 1 8 18)"}))}var se=a("ySHw"),ue=a("o0o1"),me=a.n(ue),pe=(a("ls82"),a("HaE+")),ve=a("OAQO"),be=a("FVam"),de=a("ZMKu"),fe=a("bdgK"),ye=c.a.memo,Oe=c.a.useState,Ee=c.a.useRef,xe=c.a.useEffect;var he={initialOpen:{height:"auto",transition:{duration:0}},open:function(e){return{height:e,opacity:1,visibility:"visible",transition:{duration:.3}}},closed:{height:0,opacity:0,visibility:"hidden",transition:{duration:.3}}},je={open:{x:0},closed:{x:20}},ge=ye((function(e){var t,a,r=e.children,l=e.isOpen,o=(t=l,a=Ee(),xe((function(){a.current=t}),[t]),a.current),i=function(){var e=Ee(),t=Oe({height:0}),a=Object(n.a)(t,2),r=a[0],c=a[1];return xe((function(){var t=new fe.a((function(e){var t=Object(n.a)(e,1)[0];return c(t.contentRect)}));return e.current&&t.observe(e.current),function(){return t.disconnect()}}),[]),[e,r]}(),s=Object(n.a)(i,2),u=s[0],m=s[1].height;return c.a.createElement("div",null,c.a.createElement(de.a.div,{animate:l&&o===l?"initialOpen":l?"open":"closed",custom:m,variants:he},c.a.createElement(de.a.div,{variants:je,ref:u},r)))})),we=a("x5hA"),Ne=a.n(we),Ce=c.a.useState,ke=c.a.useCallback;var Ae={rest:{scale:1},pressed:{scale:.95}},_e={rest:{rotate:0},hover:{rotate:360,transition:{duration:.3}}};function Le(){return c.a.createElement(de.a.div,{className:Ne.a.refresh,variants:Ae,initial:"rest",whileHover:"hover",whileTap:"pressed"},c.a.createElement(de.a.div,{className:"flexCenter",variants:_e},c.a.createElement(ve.a,{size:16})))}var Re=Object(l.a)((function(e,t){var a=t.proxies,n=t.name,r=Object(b.d)(e),c=Object(v.b)(e),l=Object(b.c)(e),o=Object(b.b)(e),i=Object(b.f)(e);return{apiConfig:o,proxies:H(a,c,r,i),isOpen:l["proxyProvider:".concat(n)]}}))((function(e){var t=e.name,a=e.proxies,r=e.vehicleType,i=e.updatedAt,s=e.isOpen,u=e.dispatch,m=e.apiConfig,p=Ce(!1),b=Object(n.a)(p,2),d=b[0],f=b[1],y=ke((function(){return u(Object(v.j)(m,t))}),[m,u,t]),O=ke(Object(pe.a)(me.a.mark((function e(){return me.a.wrap((function(e){for(;;)switch(e.prev=e.next){case 0:return f(!0),e.next=3,u(Object(v.f)(m,t));case 3:f(!1);case 4:case"end":return e.stop()}}),e)}))),[m,u,t,f]),x=Object(l.c)().app.updateCollapsibleIsOpen,h=ke((function(){x("proxyProvider",t,!s)}),[s,x,t]),j=Object(be.a)(new Date(i),new Date);return c.a.createElement("div",{className:Ne.a.body},c.a.createElement(E,{name:t,toggle:h,type:r,isOpen:s,qty:a.length}),c.a.createElement("div",{className:Ne.a.updatedAt},c.a.createElement("small",null,"Updated ",j," ago")),c.a.createElement(ge,{isOpen:s},c.a.createElement(U,{all:a}),c.a.createElement("div",{className:Ne.a.actionFooter},c.a.createElement(o.a,{text:"Update",start:c.a.createElement(Le,null),onClick:y}),c.a.createElement(o.a,{text:"Health Check",start:c.a.createElement(se.a,{size:16}),onClick:O,isLoading:d}))),c.a.createElement(ge,{isOpen:!s},c.a.createElement(F,{all:a})))}));var Se=function(e){var t=e.items;return 0===t.length?null:c.a.createElement(c.a.Fragment,null,c.a.createElement(i.a,{title:"Proxy Provider"}),c.a.createElement("div",null,t.map((function(e){return c.a.createElement(Re,{key:e.name,name:e.name,proxies:e.proxies,type:e.type,vehicleType:e.vehicleType,updatedAt:e.updatedAt})}))))},De=a("9cvt"),Pe=a("L5YO"),Be=a.n(Pe),Te=c.a.useState,Me=c.a.useEffect,Ue=c.a.useCallback,Ke=c.a.useRef;t.default=Object(l.a)((function(e){return{apiConfig:Object(b.b)(e),groupNames:Object(v.d)(e),proxyProviders:Object(v.e)(e),delay:Object(v.b)(e)}}))((function(e){var t=e.dispatch,a=e.groupNames,r=e.delay,l=e.proxyProviders,s=e.apiConfig,u=Ke({}),m=Ue((function(){return t(Object(v.h)(s))}),[s,t]),p=Ue((function(){u.current.startAt=new Date,t(Object(v.a)(s)).then((function(){u.current.completeAt=new Date}))}),[s,t]);Me((function(){p();var e=function(){u.current.startAt&&new Date-u.current.startAt>3e4&&p()};return window.addEventListener("focus",e,!1),function(){return window.removeEventListener("focus",e,!1)}}),[p]);var b=Te(!1),d=Object(n.a)(b,2),f=d[0],y=d[1],O=Ue((function(){y(!1)}),[]);return c.a.createElement(c.a.Fragment,null,c.a.createElement("div",{className:Be.a.topBar},c.a.createElement(o.a,{kind:"minimal",onClick:function(){return y(!0)}},c.a.createElement(ie,{size:16}))),c.a.createElement(G,{isOpen:f,onRequestClose:O},c.a.createElement(oe,null)),c.a.createElement(i.a,{title:"Proxies"}),c.a.createElement("div",null,a.map((function(e){return c.a.createElement("div",{className:Be.a.group,key:e},c.a.createElement(I,{name:e,delay:r,apiConfig:s,dispatch:t}))}))),c.a.createElement(Se,{items:l}),c.a.createElement("div",{style:{height:60}}),c.a.createElement(De.b,{icon:c.a.createElement(se.a,{width:16}),onClick:m,text:"Test Latency",position:De.c}))}))},x5hA:function(e,t,a){e.exports={updatedAt:"_3GVE9k27aM",body:"_1PV2l5z2zN",actionFooter:"_1b5XrAhEUm",refresh:"_2t6Q6BkZ73"}}}]);