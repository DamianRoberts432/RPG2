Hero sprites (optional)

Put hand-drawn pictures of the hero here, one per direction, as .bmp files:

  hero_s.bmp    facing you (down the screen)
  hero_sw.bmp   down-left
  hero_w.bmp    left
  hero_nw.bmp   up-left
  hero_n.bmp    back to you (up the screen)
  hero_ne.bmp   up-right
  hero_e.bmp    right
  hero_se.bmp   down-right

- Magenta (255, 0, 255) is see-through.
- Draw the feet at the bottom edge of the picture. It is scaled to about
  36 pixels tall in play (bigger in menus), keeping its width/height shape.
- Any size works; something like 24x36 or 32x48 keeps pixels crisp.
- If a right-side picture (hero_e, hero_ne, hero_se) is missing, the game
  mirrors its left-side twin. Any other missing direction falls back to the
  built-in drawn hero for that direction.
- Weapons, slashes and the bow-charge ring are still drawn on top.
- Pictures are read when the game starts; restart it after changing them.
