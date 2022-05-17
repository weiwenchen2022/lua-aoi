local aoi = require "aoi"

local objects = {}

local function init_obj(id, x, y, vx, vy, mode)
    objects[id] = {
	pos = {x, y, 0,},
	v = {vx, vy, 0,},
	mode = mode,
    }
end

local function update_obj(space, id)
    local obj = objects[id]

    for i = 1, 3 do
        obj.pos[i] = obj.pos[i] + obj.v[i]

        if obj.pos[i] < 0 then
            obj.pos[i] = obj.pos[i] + 100.0
        elseif obj.pos[i] > 100.0 then
            obj.pos[i] = obj.pos[i] - 100.0
        end
    end

    -- print("update_obj", id, obj.mode, table.unpack(obj.pos))
    space:update(id, obj.mode, table.unpack(obj.pos))
end


local space = aoi.new(10.0)
print(space)

init_obj(1, 40, 0, 0, 1, "w")
init_obj(2, 42, 100, 0, -1, "wm")
init_obj(3, 0, 40, 1, 0, "w")
init_obj(4, 100, 45, -1, 0, "wm")

for i = 1, 100 do
    if i < 51 then
	for j = 1, 4 do
	    update_obj(space, j)
	end
    elseif i == 51 then
	space:update(4, "d", table.unpack(objects[4].pos))
    else
	for j = 1, 3 do
	    update_obj(space, j)
	end
    end

    space:message(function(ud, watcher, marker)
	io.stderr:write(string.format("%d (%.0f, %.0f) => %d (%.0f, %.0f)\n",
	    watcher, objects[watcher].pos[1], objects[watcher].pos[2],
	    marker, objects[marker].pos[1], objects[marker].pos[2]
	))
    end)
end

space:close()
