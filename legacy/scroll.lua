--- MouseKeys scroll
--- Time-based ramp while holding, momentum glide on release

local scroll = {}

local config = nil

local pressTime = { up = nil, down = nil, left = nil, right = nil }
local velocity = { x = 0, y = 0 }
local accum = { x = 0, y = 0 }
local tapFired = { x = false, y = false }

local directions = {
  up    = { x = 0,  y = 1  },
  down  = { x = 0,  y = -1 },
  left  = { x = -1, y = 0  },
  right = { x = 1,  y = 0  },
}

function scroll.init(cfg)
  config = cfg
end

function scroll.update(now, dt)
  local anyKeyY = pressTime.up or pressTime.down
  local anyKeyX = pressTime.left or pressTime.right
  local anyKey = anyKeyY or anyKeyX

  local pixelScale = config.scrollPixelScale or 4

  -- While holding: compute speed from hold duration (time-based, no friction)
  -- On release: coast using last velocity with friction decay
  if anyKey then
    local sx, sy = 0, 0
    for dir, startTime in pairs(pressTime) do
      if startTime then
        local holdTime = now - startTime
        local speed = config.scrollInitialSpeed + config.scrollAcceleration * holdTime
        speed = math.min(speed, config.scrollMaxSpeed)
        local vec = directions[dir]
        sx = sx + vec.x * speed
        sy = sy + vec.y * speed
      end
    end
    -- Set velocity for glide on release
    velocity.x = sx
    velocity.y = sy
  else
    -- Glide: friction decay
    local friction = config.scrollFriction
    local frictionDt = friction ^ (dt / 0.016)
    velocity.x = velocity.x * frictionDt
    velocity.y = velocity.y * frictionDt
  end

  -- Tap: instant scroll on first press
  local scrollAmountX, scrollAmountY = 0, 0

  if not tapFired.y and anyKeyY then
    tapFired.y = true
    local tap = (config.scrollTap or 1) * pixelScale
    scrollAmountY = pressTime.up and tap or -tap
  end

  if not tapFired.x and anyKeyX then
    tapFired.x = true
    local tap = (config.scrollTap or 1) * pixelScale
    scrollAmountX = pressTime.right and tap or -tap
  end

  -- Accumulate and extract integer pixels
  accum.x = accum.x + velocity.x * dt * pixelScale
  accum.y = accum.y + velocity.y * dt * pixelScale

  local dx = (accum.x >= 0) and math.floor(accum.x) or math.ceil(accum.x)
  local dy = (accum.y >= 0) and math.floor(accum.y) or math.ceil(accum.y)
  accum.x = accum.x - dx
  accum.y = accum.y - dy

  scrollAmountX = scrollAmountX + dx
  scrollAmountY = scrollAmountY + dy

  if scrollAmountY ~= 0 or scrollAmountX ~= 0 then
    hs.eventtap.scrollWheel({ scrollAmountX, scrollAmountY }, {}, "pixel")
  end

  -- Reset tap state when axis idle
  if not anyKeyY and math.abs(velocity.y) < 0.1 then
    velocity.y = 0
    accum.y = 0
    tapFired.y = false
  end
  if not anyKeyX and math.abs(velocity.x) < 0.1 then
    velocity.x = 0
    accum.x = 0
    tapFired.x = false
  end
end

function scroll.press(dir)
  if not pressTime[dir] then
    pressTime[dir] = hs.timer.absoluteTime() / 1e9
  end
end

function scroll.release(dir)
  pressTime[dir] = nil
end

function scroll.releaseAll()
  for dir in pairs(pressTime) do
    pressTime[dir] = nil
  end
end

function scroll.isIdle()
  for _, startTime in pairs(pressTime) do
    if startTime then return false end
  end
  return math.abs(velocity.x) < 0.1 and math.abs(velocity.y) < 0.1
end

function scroll.stop()
  scroll.releaseAll()
  velocity.x = 0
  velocity.y = 0
  accum.x = 0
  accum.y = 0
  tapFired.x = false
  tapFired.y = false
end

return scroll
