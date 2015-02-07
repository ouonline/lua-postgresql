local pgsql = require('luapgsql')

local dbarg = {
    host = "127.0.0.1", -- required
    port = 5432, -- required
    user = "postgres", -- optional
    password = "postgres", -- optional
    db = "postgres", -- optional
    connect_timeout = 1, -- optional
}

-------------------------------------------------------------------------------

client, errmsg = pgsql.newclient(dbarg)
if errmsg ~= nil then
    io.write("connect to pgsql error: ", errmsg, "\n")
    return
end

errmsg = client:setcharset("utf8")
if errmsg ~= nil then
    io.write("setcharset error: ", errmsg, "\n")
    return
end

errmsg = client:ping()
if errmsg ~= nil then
    io.write("ping: ", errmsg, "\n")
    return
end

result, errmsg = client:query("select * from test")
if errmsg ~= nil then
    io.write("query error: ", errmsg, "\n")
    return
end

io.write("result count = ", result:count(), "\n")

fieldnamelist = result:fieldnamelist()
if fieldnamelist == nil then
    io.write("get fieldnamelist error.\n")
    return
end

for record in result:recordlist() do
    io.write("--------------------------------\n")
    for k, v in pairs(record) do
        io.write("[", k, "] -> ", fieldnamelist[k], ": ", v, "\n")
    end
end
