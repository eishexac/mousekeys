return {
  scrollInitialSpeed = 20,   -- px/sec on first frame
  scrollAcceleration = 800,  -- px/sec² ramp rate
  scrollMaxSpeed = 600,      -- px/sec cap
  scrollFriction = 0.85,     -- glide decay after release (0–1, higher = longer glide)
  scrollTap = 1,             -- instant scroll on first press (pixels * pixelScale)
  scrollPixelScale = 4,
  scrollUp = { primary = ";" },
  scrollDown = { primary = "/" },
  scrollLeft = { primary = "'" },
  scrollRight = { primary = "\\" },
}
