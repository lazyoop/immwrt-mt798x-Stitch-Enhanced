-- Licensed to the public under the GNU General Public License v3.

local m, s, o
local shadowsocksr = "shadowsocksr"


local uci = luci.model.uci.cursor()
local server_count = 0
local server_table = {}
uci:foreach("shadowsocksr", "servers", function(s)
    server_count = server_count + 1
    s["name"] = s[".name"]
    table.insert(server_table, s)
end)

local name = ""
uci:foreach("shadowsocksr", "global", function(s) name = s[".name"] end)

m = Map(shadowsocksr)

m:section(SimpleSection).template = "shadowsocksr/status"

-- [[ Servers List ]]--
s = m:section(TypedSection, "servers")
s.anonymous = true
s.addremove = true
s.sortable = false

s.des = server_count
s.current = uci:get("shadowsocksr", name, "global_server")
s.template = "cbi/add"
s.extedit = luci.dispatcher.build_url("admin/vpn/shadowsocksr/servers/%s")
function s.create(...)
	local sid = TypedSection.create(...)
    if sid then
        luci.http.redirect(s.extedit % sid)
        return
    end
end

o = s:option(DummyValue, "type", translate("Type"))
function o.cfgvalue(...) return Value.cfgvalue(...) or translate("") end

o = s:option(DummyValue, "alias", translate("Alias"))
function o.cfgvalue(...) return Value.cfgvalue(...) or translate("None") end

o = s:option(DummyValue, "server", translate("Server Address"))
function o.cfgvalue(...) return Value.cfgvalue(...) or "?" end

o = s:option(DummyValue, "server_port", translate("Server Port"))
function o.cfgvalue(...) return Value.cfgvalue(...) or "?" end

if nixio.fs.access("/usr/bin/kcptun-client") then

    o = s:option(DummyValue, "kcp_enable", translate("KcpTun"))
    function o.cfgvalue(...) return Value.cfgvalue(...) or "?" end

end


return m

