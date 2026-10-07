-- 用 os.execute 列出 app/ 下的子目录
local tmpfile = os.tmpname()
os.execute("ls -d app/*/ > " .. tmpfile .. " 2>/dev/null")

local f = io.open(tmpfile, "r")
if f then
    for line in f:lines() do
        local path = line:gsub("/+$", "")
        local name = path:match("([^/]+)$")
        if name then
            print("[test] loading " .. path .. " as " .. name)
            sys.load(path, name)
        end
    end
    f:close()
end
os.remove(tmpfile)

print("[test] showing app 'label'")
sys.show("label")

print("[test] calling sys.ls()")
sys.ls()
