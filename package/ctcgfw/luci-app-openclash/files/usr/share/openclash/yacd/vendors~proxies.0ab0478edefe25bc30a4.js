(window.webpackJsonp=window.webpackJsonp||[]).push([[7],{"8/mu":function(t,e,n){"use strict";var r=n("q1tI"),o=n.n(r),i=n("17x9"),s=n.n(i);function a(){return(a=Object.assign||function(t){for(var e=1;e<arguments.length;e++){var n=arguments[e];for(var r in n)Object.prototype.hasOwnProperty.call(n,r)&&(t[r]=n[r])}return t}).apply(this,arguments)}function c(t,e){if(null==t)return{};var n,r,o=function(t,e){if(null==t)return{};var n,r,o={},i=Object.keys(t);for(r=0;r<i.length;r++)n=i[r],e.indexOf(n)>=0||(o[n]=t[n]);return o}(t,e);if(Object.getOwnPropertySymbols){var i=Object.getOwnPropertySymbols(t);for(r=0;r<i.length;r++)n=i[r],e.indexOf(n)>=0||Object.prototype.propertyIsEnumerable.call(t,n)&&(o[n]=t[n])}return o}var u=function(t){var e=t.color,n=t.size,r=c(t,["color","size"]);return o.a.createElement("svg",a({xmlns:"http://www.w3.org/2000/svg",width:n,height:n,viewBox:"0 0 24 24",fill:"none",stroke:e,strokeWidth:"2",strokeLinecap:"round",strokeLinejoin:"round"},r),o.a.createElement("circle",{cx:"12",cy:"12",r:"10"}))};u.propTypes={color:s.a.string,size:s.a.oneOfType([s.a.string,s.a.number])},u.defaultProps={color:"currentColor",size:"24"},e.a=u},OAQO:function(t,e,n){"use strict";var r=n("q1tI"),o=n.n(r),i=n("17x9"),s=n.n(i);function a(){return(a=Object.assign||function(t){for(var e=1;e<arguments.length;e++){var n=arguments[e];for(var r in n)Object.prototype.hasOwnProperty.call(n,r)&&(t[r]=n[r])}return t}).apply(this,arguments)}function c(t,e){if(null==t)return{};var n,r,o=function(t,e){if(null==t)return{};var n,r,o={},i=Object.keys(t);for(r=0;r<i.length;r++)n=i[r],e.indexOf(n)>=0||(o[n]=t[n]);return o}(t,e);if(Object.getOwnPropertySymbols){var i=Object.getOwnPropertySymbols(t);for(r=0;r<i.length;r++)n=i[r],e.indexOf(n)>=0||Object.prototype.propertyIsEnumerable.call(t,n)&&(o[n]=t[n])}return o}var u=function(t){var e=t.color,n=t.size,r=c(t,["color","size"]);return o.a.createElement("svg",a({xmlns:"http://www.w3.org/2000/svg",width:n,height:n,viewBox:"0 0 24 24",fill:"none",stroke:e,strokeWidth:"2",strokeLinecap:"round",strokeLinejoin:"round"},r),o.a.createElement("polyline",{points:"23 4 23 10 17 10"}),o.a.createElement("path",{d:"M20.49 15a9 9 0 1 1-2.12-9.36L23 10"}))};u.propTypes={color:s.a.string,size:s.a.oneOfType([s.a.string,s.a.number])},u.defaultProps={color:"currentColor",size:"24"},e.a=u},bdgK:function(t,e,n){"use strict";(function(t){var n=function(){if("undefined"!=typeof Map)return Map;function t(t,e){var n=-1;return t.some((function(t,r){return t[0]===e&&(n=r,!0)})),n}return(function(){function e(){this.__entries__=[]}return Object.defineProperty(e.prototype,"size",{get:function(){return this.__entries__.length},enumerable:!0,configurable:!0}),e.prototype.get=function(e){var n=t(this.__entries__,e),r=this.__entries__[n];return r&&r[1]},e.prototype.set=function(e,n){var r=t(this.__entries__,e);~r?this.__entries__[r][1]=n:this.__entries__.push([e,n])},e.prototype.delete=function(e){var n=this.__entries__,r=t(n,e);~r&&n.splice(r,1)},e.prototype.has=function(e){return!!~t(this.__entries__,e)},e.prototype.clear=function(){this.__entries__.splice(0)},e.prototype.forEach=function(t,e){void 0===e&&(e=null);for(var n=0,r=this.__entries__;n<r.length;n++){var o=r[n];t.call(e,o[1],o[0])}},e}())}(),r="undefined"!=typeof window&&"undefined"!=typeof document&&window.document===document,o=void 0!==t&&t.Math===Math?t:"undefined"!=typeof self&&self.Math===Math?self:"undefined"!=typeof window&&window.Math===Math?window:Function("return this")(),i="function"==typeof requestAnimationFrame?requestAnimationFrame.bind(o):function(t){return setTimeout((function(){return t(Date.now())}),1e3/60)};var s=["top","right","bottom","left","width","height","size","weight"],a="undefined"!=typeof MutationObserver,c=function(){function t(){this.connected_=!1,this.mutationEventsAdded_=!1,this.mutationsObserver_=null,this.observers_=[],this.onTransitionEnd_=this.onTransitionEnd_.bind(this),this.refresh=function(t,e){var n=!1,r=!1,o=0;function s(){n&&(n=!1,t()),r&&c()}function a(){i(s)}function c(){var t=Date.now();if(n){if(t-o<2)return;r=!0}else n=!0,r=!1,setTimeout(a,e);o=t}return c}(this.refresh.bind(this),20)}return t.prototype.addObserver=function(t){~this.observers_.indexOf(t)||this.observers_.push(t),this.connected_||this.connect_()},t.prototype.removeObserver=function(t){var e=this.observers_,n=e.indexOf(t);~n&&e.splice(n,1),!e.length&&this.connected_&&this.disconnect_()},t.prototype.refresh=function(){this.updateObservers_()&&this.refresh()},t.prototype.updateObservers_=function(){var t=this.observers_.filter((function(t){return t.gatherActive(),t.hasActive()}));return t.forEach((function(t){return t.broadcastActive()})),t.length>0},t.prototype.connect_=function(){r&&!this.connected_&&(document.addEventListener("transitionend",this.onTransitionEnd_),window.addEventListener("resize",this.refresh),a?(this.mutationsObserver_=new MutationObserver(this.refresh),this.mutationsObserver_.observe(document,{attributes:!0,childList:!0,characterData:!0,subtree:!0})):(document.addEventListener("DOMSubtreeModified",this.refresh),this.mutationEventsAdded_=!0),this.connected_=!0)},t.prototype.disconnect_=function(){r&&this.connected_&&(document.removeEventListener("transitionend",this.onTransitionEnd_),window.removeEventListener("resize",this.refresh),this.mutationsObserver_&&this.mutationsObserver_.disconnect(),this.mutationEventsAdded_&&document.removeEventListener("DOMSubtreeModified",this.refresh),this.mutationsObserver_=null,this.mutationEventsAdded_=!1,this.connected_=!1)},t.prototype.onTransitionEnd_=function(t){var e=t.propertyName,n=void 0===e?"":e;s.some((function(t){return!!~n.indexOf(t)}))&&this.refresh()},t.getInstance=function(){return this.instance_||(this.instance_=new t),this.instance_},t.instance_=null,t}(),u=function(t,e){for(var n=0,r=Object.keys(e);n<r.length;n++){var o=r[n];Object.defineProperty(t,o,{value:e[o],enumerable:!1,writable:!1,configurable:!0})}return t},h=function(t){return t&&t.ownerDocument&&t.ownerDocument.defaultView||o},l=y(0,0,0,0);function f(t){return parseFloat(t)||0}function p(t){for(var e=[],n=1;n<arguments.length;n++)e[n-1]=arguments[n];return e.reduce((function(e,n){return e+f(t["border-"+n+"-width"])}),0)}function d(t){var e=t.clientWidth,n=t.clientHeight;if(!e&&!n)return l;var r=h(t).getComputedStyle(t),o=function(t){for(var e={},n=0,r=["top","right","bottom","left"];n<r.length;n++){var o=r[n],i=t["padding-"+o];e[o]=f(i)}return e}(r),i=o.left+o.right,s=o.top+o.bottom,a=f(r.width),c=f(r.height);if("border-box"===r.boxSizing&&(Math.round(a+i)!==e&&(a-=p(r,"left","right")+i),Math.round(c+s)!==n&&(c-=p(r,"top","bottom")+s)),!function(t){return t===h(t).document.documentElement}(t)){var u=Math.round(a+i)-e,d=Math.round(c+s)-n;1!==Math.abs(u)&&(a-=u),1!==Math.abs(d)&&(c-=d)}return y(o.left,o.top,a,c)}var v="undefined"!=typeof SVGGraphicsElement?function(t){return t instanceof h(t).SVGGraphicsElement}:function(t){return t instanceof h(t).SVGElement&&"function"==typeof t.getBBox};function b(t){return r?v(t)?function(t){var e=t.getBBox();return y(0,0,e.width,e.height)}(t):d(t):l}function y(t,e,n,r){return{x:t,y:e,width:n,height:r}}var m=function(){function t(t){this.broadcastWidth=0,this.broadcastHeight=0,this.contentRect_=y(0,0,0,0),this.target=t}return t.prototype.isActive=function(){var t=b(this.target);return this.contentRect_=t,t.width!==this.broadcastWidth||t.height!==this.broadcastHeight},t.prototype.broadcastRect=function(){var t=this.contentRect_;return this.broadcastWidth=t.width,this.broadcastHeight=t.height,t},t}(),_=function(t,e){var n,r,o,i,s,a,c,h=(r=(n=e).x,o=n.y,i=n.width,s=n.height,a="undefined"!=typeof DOMRectReadOnly?DOMRectReadOnly:Object,c=Object.create(a.prototype),u(c,{x:r,y:o,width:i,height:s,top:o,right:r+i,bottom:s+o,left:r}),c);u(this,{target:t,contentRect:h})},g=function(){function t(t,e,r){if(this.activeObservations_=[],this.observations_=new n,"function"!=typeof t)throw new TypeError("The callback provided as parameter 1 is not a function.");this.callback_=t,this.controller_=e,this.callbackCtx_=r}return t.prototype.observe=function(t){if(!arguments.length)throw new TypeError("1 argument required, but only 0 present.");if("undefined"!=typeof Element&&Element instanceof Object){if(!(t instanceof h(t).Element))throw new TypeError('parameter 1 is not of type "Element".');var e=this.observations_;e.has(t)||(e.set(t,new m(t)),this.controller_.addObserver(this),this.controller_.refresh())}},t.prototype.unobserve=function(t){if(!arguments.length)throw new TypeError("1 argument required, but only 0 present.");if("undefined"!=typeof Element&&Element instanceof Object){if(!(t instanceof h(t).Element))throw new TypeError('parameter 1 is not of type "Element".');var e=this.observations_;e.has(t)&&(e.delete(t),e.size||this.controller_.removeObserver(this))}},t.prototype.disconnect=function(){this.clearActive(),this.observations_.clear(),this.controller_.removeObserver(this)},t.prototype.gatherActive=function(){var t=this;this.clearActive(),this.observations_.forEach((function(e){e.isActive()&&t.activeObservations_.push(e)}))},t.prototype.broadcastActive=function(){if(this.hasActive()){var t=this.callbackCtx_,e=this.activeObservations_.map((function(t){return new _(t.target,t.broadcastRect())}));this.callback_.call(t,e,t),this.clearActive()}},t.prototype.clearActive=function(){this.activeObservations_.splice(0)},t.prototype.hasActive=function(){return this.activeObservations_.length>0},t}(),w="undefined"!=typeof WeakMap?new WeakMap:new n,O=function t(e){if(!(this instanceof t))throw new TypeError("Cannot call a class as a function.");if(!arguments.length)throw new TypeError("1 argument required, but only 0 present.");var n=c.getInstance(),r=new g(e,n,this);w.set(this,r)};["observe","unobserve","disconnect"].forEach((function(t){O.prototype[t]=function(){var e;return(e=w.get(this))[t].apply(e,arguments)}}));var E=void 0!==o.ResizeObserver?o.ResizeObserver:O;e.a=E}).call(this,n("yLpj"))},cq0i:function(t,e,n){"use strict";n.d(e,"a",(function(){return s})),n.d(e,"b",(function(){return u}));var r=n("q1tI"),o=n.n(r);function i(){return(i=Object.assign||function(t){for(var e=1;e<arguments.length;e++){var n=arguments[e];for(var r in n)Object.prototype.hasOwnProperty.call(n,r)&&(t[r]=n[r])}return t}).apply(this,arguments)}const s=t=>o.a.createElement("button",i({type:"button"},t,{className:"rtf--ab"}),t.children),a=t=>o.a.createElement("button",i({type:"button",className:"rtf--mb"},t),t.children),c={bottom:24,right:24},u=({event:t="hover",position:e=c,alwaysShowTitle:n=!1,children:i,icon:s,mainButtonStyles:u,onClick:h,text:l})=>{const[f,p]=Object(r.useState)(!1),d=n||!f,v=()=>p(!0),b=()=>p(!1);return o.a.createElement("ul",{onMouseEnter:()=>"hover"===t&&v(),onMouseLeave:()=>"hover"===t&&b(),className:`rtf ${f?"open":"closed"}`,style:e},o.a.createElement("li",{className:"rtf--mb__c"},o.a.createElement(a,{onClick:e=>h?h(e):(e.persist(),"click"===t?f?b():v():null),style:u,role:"button","aria-label":"Floating menu",tabIndex:"0"},s),h&&l&&o.a.createElement("span",{className:`${"right"in e?"right":""} ${n?"always-show":""}`,"aria-hidden":d},l),o.a.createElement("ul",null,(6<o.a.Children.count(i)&&console.warn("react-tiny-fab only supports up to 6 action buttons"),o.a.Children.map(i,(t,r)=>t&&o.a.createElement("li",{className:`rtf--ab__c ${"top"in e?"top":""}`},o.a.cloneElement(t,{"data-testid":`action-button-${r}`,"aria-label":t.props.text||`Menu button ${r+1}`,"aria-hidden":d,tabIndex:f?0:-1,...t.props,onClick:()=>(t=>{p(!1),setTimeout(()=>{t()},1)})(t.props.onClick)}),t.props.text&&o.a.createElement("span",{className:`${"right"in e?"right":""} ${n?"always-show":""}`,"aria-hidden":d},t.props.text)))))))}},fIvA:function(t,e,n){"use strict";var r=n("q1tI"),o=n.n(r),i=n("17x9"),s=n.n(i);function a(){return(a=Object.assign||function(t){for(var e=1;e<arguments.length;e++){var n=arguments[e];for(var r in n)Object.prototype.hasOwnProperty.call(n,r)&&(t[r]=n[r])}return t}).apply(this,arguments)}function c(t,e){if(null==t)return{};var n,r,o=function(t,e){if(null==t)return{};var n,r,o={},i=Object.keys(t);for(r=0;r<i.length;r++)n=i[r],e.indexOf(n)>=0||(o[n]=t[n]);return o}(t,e);if(Object.getOwnPropertySymbols){var i=Object.getOwnPropertySymbols(t);for(r=0;r<i.length;r++)n=i[r],e.indexOf(n)>=0||Object.prototype.propertyIsEnumerable.call(t,n)&&(o[n]=t[n])}return o}var u=function(t){var e=t.color,n=t.size,r=c(t,["color","size"]);return o.a.createElement("svg",a({xmlns:"http://www.w3.org/2000/svg",width:n,height:n,viewBox:"0 0 24 24",fill:"none",stroke:e,strokeWidth:"2",strokeLinecap:"round",strokeLinejoin:"round"},r),o.a.createElement("polygon",{points:"22 3 2 3 10 12.46 10 19 14 21 14 12.46 22 3"}))};u.propTypes={color:s.a.string,size:s.a.oneOfType([s.a.string,s.a.number])},u.defaultProps={color:"currentColor",size:"24"},e.a=u}}]);