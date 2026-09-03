--- MouseKeys movement
--- Time-based cursor movement with no momentum

local movement = {}

local config = nil
local clickModule = nil
local scale = { x = 1, y = 1 }
local screenWatcher = nil

local pressTime = { up = nil, down = nil, left = nil, right = nil }
local moveAccum = { x = 0, y = 0 }

local directions = {
  up    = { x = 0,  y = -1 },
  down  = { x = 0,  y = 1  },
  left  = { x = -1, y = 0  },
  right = { x = 1,  y = 0  },
}

-- Compute independent X/Y scale factors based on total screen bounding box.
-- Uses the primary screen's landscape-oriented dimensions as the dynamic
-- reference so that speed feels consistent regardless of orientation or
-- monitor count.
local function computeScale()
  local primary = hs.screen.primaryScreen():fullFrame()
  -- Use landscape dimensions (max,min) so a rotated monitor still gets
  -- the correct reference — a vertical 1080×1920 yields ref 1920×1080.
  local refW = math.max(primary.w, primary.h)
  local refH = math.min(primary.w, primary.h)

  local screens = hs.screen.allScreens()
  local minX, minY = math.huge, math.huge
  local maxX, maxY = -math.huge, -math.huge
  for _, s in ipairs(screens) do
    local f = s:fullFrame()
    minX = math.min(minX, f.x)
    minY = math.min(minY, f.y)
    maxX = math.max(maxX, f.x + f.w)
    maxY = math.max(maxY, f.y + f.h)
  end

  return {
    x = (maxX - minX) / refW,
    y = (maxY - minY) / refH,
  }
end

function movement.init(cfg, click)
  config = cfg
  clickModule = click
  scale = computeScale()

  if screenWatcher then screenWatcher:stop() end
  screenWatcher = hs.screen.watcher.new(function()
    scale = computeScale()
  end):start()
end

function movement.update(now, dt)
  local dx_speed, dy_speed = 0, 0
  local anyKey = false

  for dir, startTime in pairs(pressTime) do
    if startTime then
      anyKey = true
      local holdTime = now - startTime
      local vec = directions[dir]
      -- scale acceleration and max speed per axis based on screen layout
      local sx = (vec.x ~= 0) and scale.x or 1
      local sy = (vec.y ~= 0) and scale.y or 1
      local speedX = config.initialSpeed + config.acceleration * sx * holdTime
      speedX = math.min(speedX, config.maxSpeed * sx)
      local speedY = config.initialSpeed + config.acceleration * sy * holdTime
      speedY = math.min(speedY, config.maxSpeed * sy)
      dx_speed = dx_speed + vec.x * speedX
      dy_speed = dy_speed + vec.y * speedY
    end
  end

  if not anyKey then
    moveAccum.x = 0
    moveAccum.y = 0
    return
  end

  moveAccum.x = moveAccum.x + dx_speed * dt
  moveAccum.y = moveAccum.y + dy_speed * dt

  local dx = (moveAccum.x >= 0) and math.floor(moveAccum.x) or math.ceil(moveAccum.x)
  local dy = (moveAccum.y >= 0) and math.floor(moveAccum.y) or math.ceil(moveAccum.y)

  if dx ~= 0 or dy ~= 0 then
    moveAccum.x = moveAccum.x - dx
    moveAccum.y = moveAccum.y - dy

    local pos = hs.mouse.absolutePosition()

    -- clamp to the bounding box of all screens so the cursor
    -- can move across displays but not off into void
    local minX, minY = math.huge, math.huge
    local maxX, maxY = -math.huge, -math.huge
    for _, s in ipairs(hs.screen.allScreens()) do
      local f = s:fullFrame()
      minX = math.min(minX, f.x)
      minY = math.min(minY, f.y)
      maxX = math.max(maxX, f.x + f.w - 1)
      maxY = math.max(maxY, f.y + f.h - 1)
    end

    local newPos = {
      x = math.max(minX, math.min(maxX, pos.x + dx)),
      y = math.max(minY, math.min(maxY, pos.y + dy))
    }

    if newPos.x ~= pos.x or newPos.y ~= pos.y then
      local moveType = (clickModule and clickModule.isLeftDown())
        and hs.eventtap.event.types.leftMouseDragged
        or hs.eventtap.event.types.mouseMoved
      hs.eventtap.event.newMouseEvent(moveType, newPos):post()
    end
  end
end

function movement.press(dir)
  if not pressTime[dir] then
    pressTime[dir] = hs.timer.absoluteTime() / 1e9
  end
end

function movement.release(dir)
  pressTime[dir] = nil
end

function movement.releaseAll()
  for dir in pairs(pressTime) do
    pressTime[dir] = nil
  end
  moveAccum.x = 0
  moveAccum.y = 0
end

function movement.isIdle()
  for _, startTime in pairs(pressTime) do
    if startTime then return false end
  end
  return true
end

function movement.stop()
  movement.releaseAll()
  if screenWatcher then
    screenWatcher:stop()
    screenWatcher = nil
  end
end

return movement
