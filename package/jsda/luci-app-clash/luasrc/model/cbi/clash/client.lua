local NXFS = require "nixio.fs"
local SYS  = require "luci.sys"
local HTTP = require "luci.http"
local DISP = require "luci.dispatcher"
local UTIL = require "luci.util"
local uci = require("luci.model.uci").cursor()
local fs = require "luci.clash"
local clash = "clash"


m = Map("clash")
s = m:section(TypedSection, "clash")
s.anonymous = true
m.pageaction = false

o = s:option(ListValue, "core", translate("Core"))
o.default = "clashcore"
if nixio.fs.access("/etc/clash/clash") then
o:value("1", translate("Clash"))
end
if nixio.fs.access("/usr/bin/clash") then
o:value("2", translate("Clashr"))
end
if nixio.fs.access("/etc/clash/clashtun/clash") then
o:value("3", translate("Clash(cTun)"))
end
if nixio.fs.access("/etc/clash/dtun/clash") then
o:value("4", translate("Clash(dTun)"))
end
o.description = translate("Select core, clashr support ssr while clash does not.")

o = s:option(ListValue, "g_rules", translate("Game Rules"))
o.default = "0"
o:value("0", translate("Disable"))
o:value("1", translate("Enable"))
o.description = translate("Set rules under Setting=>Game Rules, will take effect when client start")


o = s:option(Button, "Apply")
o.title = translate("Save & Apply")
o.inputtitle = translate("Save & Apply")
o.inputstyle = "apply"
o.write = function()
  m.uci:commit("clash")
end

o = s:option(Button,"action")
o.title = translate("Operation")
o.template = "clash/start_stop"


return m

