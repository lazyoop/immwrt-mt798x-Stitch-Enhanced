module("luci.model.cbi.brook_server.api.brook", package.seeall)
local fs = require "nixio.fs"
local sys = require "luci.sys"
local uci = require"luci.model.uci".cursor()
local util = require "luci.util"
local i18n = require "luci.i18n"

local appname = "brook_server"
local brook_api =
    "https://api.github.com/repos/txthinking/brook/releases/latest"

local wget = "/usr/bin/wget"
local wget_args = {
    "--no-check-certificate", "--quiet", "--timeout=100", "--tries=3"
}
local command_timeout = 300

local LEDE_BOARD = nil
local DISTRIB_TARGET = nil
local is_armv7 = false

function uci_get_type(type, config, default)
    value = uci:get(appname, "@" .. type .. "[0]", config) or sys.exec(
                "echo -n `uci -q get " .. appname .. ".@" .. type .. "[0]." ..
                    config .. "`")
    if (value == nil or value == "") and (default and default ~= "") then
        value = default
    end
    return value
end

local function _unpack(t, i)
    i = i or 1
    if t[i] ~= nil then return t[i], _unpack(t, i + 1) end
end

local function exec(cmd, args, writer, timeout)
    local os = require "os"
    local nixio = require "nixio"

    local fdi, fdo = nixio.pipe()
    local pid = nixio.fork()

    if pid > 0 then
        fdo:close()

        if writer or timeout then
            local starttime = os.time()
            while true do
                if timeout and os.difftime(os.time(), starttime) >= timeout then
                    nixio.kill(pid, nixio.const.SIGTERM)
                    return 1
                end

                if writer then
                    local buffer = fdi:read(2048)
                    if buffer and #buffer > 0 then
                        writer(buffer)
                    end
                end

                local wpid, stat, code = nixio.waitpid(pid, "nohang")

                if wpid and stat == "exited" then return code end

                if not writer and timeout then nixio.nanosleep(1) end
            end
        else
            local wpid, stat, code = nixio.waitpid(pid)
            return wpid and stat == "exited" and code
        end
    elseif pid == 0 then
        nixio.dup(fdo, nixio.stdout)
        fdi:close()
        fdo:close()
        nixio.exece(cmd, args, nil)
        nixio.stdout:close()
        os.exit(1)
    end
end

local function compare_versions(ver1, comp, ver2)
    local table = table

    local av1 = util.split(ver1, "[%.%-]", nil, true)
    local av2 = util.split(ver2, "[%.%-]", nil, true)

    local max = table.getn(av1)
    local n2 = table.getn(av2)
    if (max < n2) then max = n2 end

    for i = 1, max, 1 do
        local s1 = av1[i] or ""
        local s2 = av2[i] or ""

        if comp == "~=" and (s1 ~= s2) then return true end
        if (comp == "<" or comp == "<=") and (s1 < s2) then return true end
        if (comp == ">" or comp == ">=") and (s1 > s2) then return true end
        if (s1 ~= s2) then return false end
    end

    return not (comp == "<" or comp == ">")
end

local function auto_get_arch()
    local arch = nixio.uname().machine or ""
    if fs.access("/usr/lib/os-release") then
        LEDE_BOARD = sys.exec(
                         "echo -n `grep 'LEDE_BOARD' /usr/lib/os-release | awk -F '[\\042\\047]' '{print $2}'`")
    end
    if fs.access("/etc/openwrt_release") then
        DISTRIB_TARGET = sys.exec(
                             "echo -n `grep 'DISTRIB_TARGET' /etc/openwrt_release | awk -F '[\\042\\047]' '{print $2}'`")
    end

    if arch == "mips" then
        if LEDE_BOARD and LEDE_BOARD ~= "" then
            if string.match(LEDE_BOARD, "ramips") == "ramips" then
                arch = "ramips"
            else
                arch = sys.exec("echo '" .. LEDE_BOARD ..
                                    "' | grep -oE 'ramips|ar71xx'")
            end
        elseif DISTRIB_TARGET and DISTRIB_TARGET ~= "" then
            if string.match(DISTRIB_TARGET, "ramips") == "ramips" then
                arch = "ramips"
            else
                arch = sys.exec("echo '" .. DISTRIB_TARGET ..
                                    "' | grep -oE 'ramips|ar71xx'")
            end
        end
    end

    return util.trim(arch)
end

local function get_file_info(arch)
    local file_tree = ""
    local sub_version = ""

    if arch == "x86_64" then
        file_tree = "amd64"
    elseif arch == "aarch64" then
        file_tree = "arm64"
    elseif arch == "ramips" then
        file_tree = "mipsle"
    elseif arch == "ar71xx" then
        file_tree = "mips"
    elseif arch:match("^i[%d]86$") then
        file_tree = "386"
    elseif arch:match("^armv[5-8]") then
        file_tree = "arm"
        sub_version = arch:match("[5-8]")
        if LEDE_BOARD and string.match(LEDE_BOARD, "bcm53xx") == "bcm53xx" then
            sub_version = "5"
        elseif DISTRIB_TARGET and string.match(DISTRIB_TARGET, "bcm53xx") ==
            "bcm53xx" then
            sub_version = "5"
        end
        sub_version = "5"
    end

    return file_tree, sub_version
end

function get_api_json(url)
    local jsonc = require "luci.jsonc"

    local output = {}
    -- exec(wget, { "-O-", url, _unpack(wget_args) },
    --	function(chunk) output[#output + 1] = chunk end)
    -- local json_content = util.trim(table.concat(output))

    local json_content = luci.sys.exec(wget ..
                                           " --no-check-certificate --timeout=10 -t 1 -O- " ..
                                           url)

    if json_content == "" then return {} end

    return jsonc.parse(json_content) or {}
end

function get_brook_file_path()
    return uci_get_type("global", "brook_path", "/usr/bin/brook")
end

function get_brook_version(file)
    if file == nil then file = get_brook_file_path() end

    if file and file ~= "" then
        if not fs.access(file, "rwx", "rx", "rx") then
            fs.chmod(file, 755)
        end

        local info = util.trim(sys.exec("%s -v 2>/dev/null" % file))

        if info ~= "" then
            local tb = util.split(info, "%s+", nil, true)
            return tb[1] == "Brook" and tb[3] or ""
        end
    end

    return ""
end

function to_check(arch)
    if not arch or arch == "" then arch = auto_get_arch() end

    local file_tree, sub_version = get_file_info(arch)

    if file_tree == "" then
        return {
            code = 1,
            error = i18n.translate(
                "Can't determine ARCH, or ARCH not supported.")
        }
    end

    file_tree = "_linux_" .. file_tree
    if file_tree == "_linux_amd64" then file_tree = "" end

    local json = get_api_json(brook_api)

    if json.tag_name == nil then
        return {
            code = 1,
            error = i18n.translate("Get remote version info failed.")
        }
    end

    local remote_version = json.tag_name:match("[^v]+")

    local client_file = get_brook_file_path()

    local needs_update = compare_versions(get_brook_version(client_file), "<",
                                          remote_version)
    local html_url, download_url

    if needs_update then
        html_url = json.html_url
        for _, v in ipairs(json.assets) do
            if v.name and v.name:match(file_tree .. sub_version) then
                download_url = v.browser_download_url
                break
            end
        end
    end

    if needs_update and not download_url then
        return {
            code = 1,
            now_version = get_brook_version(client_file),
            version = remote_version,
            html_url = html_url,
            error = i18n.translate(
                "New version found, but failed to get new version download url.")
        }
    end

    return {
        code = 0,
        update = needs_update,
        now_version = get_brook_version(client_file),
        version = remote_version,
        url = {html = html_url, download = download_url}
    }
end

function to_download(url)
    if not url or url == "" then
        return {code = 1, error = i18n.translate("Download url is required.")}
    end

    sys.call("/bin/rm -f /tmp/brook_download.*")

    local tmp_file = util.trim(util.exec("mktemp -u -t brook_download.XXXXXX"))

    local result = exec(wget, {"-O", tmp_file, url, _unpack(wget_args)}, nil,
                        command_timeout) == 0

    if not result then
        exec("/bin/rm", {"-f", tmp_file})
        return {
            code = 1,
            error = i18n.translatef("File download failed or timed out: %s", url)
        }
    end

    return {code = 0, file = tmp_file}
end

function to_move(file)
    if not file or file == "" or not fs.access(file) then
        sys.call("/bin/rm -rf /tmp/brook_download.*")
        return {code = 1, error = i18n.translate("Client file is required.")}
    end

    local version = get_brook_version(file)
    if version == "" then
        sys.call("/bin/rm -rf /tmp/brook_download.*")
        return {
            code = 1,
            error = i18n.translate(
                "The client file is not suitable for current device.")
        }
    end

    local client_file = get_brook_file_path()
    local client_file_bak

    if fs.access(client_file) then
        client_file_bak = client_file .. ".bak"
        exec("/bin/mv", {"-f", client_file, client_file_bak})
    end

    local result = exec("/bin/mv", {"-f", file, client_file}, nil,
                        command_timeout) == 0

    if not result or not fs.access(client_file) then
        sys.call("/bin/rm -rf /tmp/brook_download.*")
        if client_file_bak then
            exec("/bin/mv", {"-f", client_file_bak, client_file})
        end
        return {
            code = 1,
            error = i18n.translatef("Can't move new file to path: %s",
                                    client_file)
        }
    end

    exec("/bin/chmod", {"755", client_file})

    if client_file_bak then exec("/bin/rm", {"-f", client_file_bak}) end

    sys.call("/bin/rm -rf /tmp/brook_download.*")

    return {code = 0}
end
