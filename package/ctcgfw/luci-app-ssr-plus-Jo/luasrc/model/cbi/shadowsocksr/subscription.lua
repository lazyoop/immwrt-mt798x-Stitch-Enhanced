local shadowsocksr= "shadowsocksr"
local uci = luci.model.uci.cursor()
local server_table = {}
local gfwmode=0
local gfw_count=0
local ip_count=0

if nixio.fs.access("/etc/dnsmasq.ssr/gfw_list.conf") then
gfwmode=1		
end

local sys = require "luci.sys"

if gfwmode==1 then 
 gfw_count = tonumber(sys.exec("cat /etc/dnsmasq.ssr/gfw_list.conf | wc -l"))/2

end


m = Map(shadowsocksr)
m:section(SimpleSection).template  = "shadowsocksr/status3"
-- [[ 节点订阅 ]]--

s = m:section(TypedSection, "server_subscribe",  translate("Subscription"))
s.anonymous = true

o = s:option(Flag, "auto_update", translate("Auto Update"))
o.rmempty = false
o.description = translate("Auto Update Server subscription, GFW list and CHN route")


o = s:option(ListValue, "auto_update_time", translate("Update time (every day)"))
for t = 0,23 do
    o:value(t, t..":00")
end
o.default=2
o.rmempty = false

o = s:option(DynamicList, "subscribe_url", translate("Subscribe URL"),
	translate("Before subscribing please click below to delete all servers in the subscription"))
o.rmempty = true

o = s:option(Flag, "proxy", translate("Through proxy update"))
o.rmempty = false
o.description = translate("Through proxy update list, Not Recommended ")


o = s:option(DummyValue, "", "")
o.rawhtml = true
o.template = "shadowsocksr/update_subscribe"



o = s:option(Button,"delete",translate("Delete all severs"))
o.inputstyle = "reset"


o.write = function()
    uci:delete_all("shadowsocksr", "servers", function(s) return true end)
	uci:commit("shadowsocksr") 
    luci.sys.call("uci commit shadowsocksr && /etc/init.d/shadowsocksr stop")

    luci.http.redirect(luci.dispatcher.build_url("admin", "vpn", "shadowsocksr", "servers"))
 return
end



return m





