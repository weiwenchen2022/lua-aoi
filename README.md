## Area of Interest

```Lua

--[[
create a space object
]]
new(radius)

--[[
update an aoi object.

id : integer
mode : string
  "w" : watcher
  "m" : marker
  "wm" : watcher and marker
  "d" : drop an object
pos_x, pos_y, pos_z : number
]]
space:update(id, mode, pos_x, pos_y, pos_z)

--[[
define a callback function first, and call message in your timer or main loop.
]]
space:message(function(ud, watcher, marker) --[[ body ]] end, ud)

space:close()


```
See test.lua for example.
