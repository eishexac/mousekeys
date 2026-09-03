--- MouseKeys click
--- Click operations with multi-click detection

local click = {}

local leftHeld = false
local lastClickTime = 0
local clickCount = 0
local doubleClickInterval = 0.3

function click.leftDown()
  if not leftHeld then
    leftHeld = true
    local now = hs.timer.absoluteTime() / 1e9
    if (now - lastClickTime) < doubleClickInterval then
      clickCount = clickCount + 1
    else
      clickCount = 1
    end
    lastClickTime = now
    local pos = hs.mouse.absolutePosition()
    local event = hs.eventtap.event.newMouseEvent(hs.eventtap.event.types.leftMouseDown, pos)
    event:setProperty(hs.eventtap.event.properties.mouseEventClickState, clickCount)
    event:post()
  end
end

function click.leftUp()
  if leftHeld then
    leftHeld = false
    local pos = hs.mouse.absolutePosition()
    local event = hs.eventtap.event.newMouseEvent(hs.eventtap.event.types.leftMouseUp, pos)
    event:setProperty(hs.eventtap.event.properties.mouseEventClickState, clickCount)
    event:post()
  end
end

function click.isLeftDown()
  return leftHeld
end

function click.rightClick()
  hs.eventtap.rightClick(hs.mouse.absolutePosition())
end

return click
