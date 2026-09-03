--- mousekeys.spoon
--- Mouse control using keyboard

local obj = {}
obj.__index = obj

obj.name = "mousekeys"
obj.version = "0.2.0"
obj.author = "hexac"

-- Load default configs
local spoonPath = hs.spoons.resourcePath("")

local function loadConfig(name)
  local defaults = dofile(spoonPath .. "/configs/" .. name .. ".lua")
  for k, v in pairs(defaults) do
    obj[k] = v
  end
end

loadConfig("move")
loadConfig("click")
loadConfig("scroll")
loadConfig("layer")

-- Load modules
local movement = dofile(spoonPath .. "/movement.lua")
local scroll = dofile(spoonPath .. "/scroll.lua")
local click = dofile(spoonPath .. "/click.lua")
local layer = dofile(spoonPath .. "/layer.lua")

function obj:start()
  layer.init(self, {
    movement = movement,
    scroll = scroll,
    click = click,
  })
  layer.start()
  return self
end

function obj:stop()
  layer.stop()
end

return obj
