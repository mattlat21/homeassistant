# Home Assistant icon font (MDI)

`ui_font_home_assistant_icons_56.c` is generated with [lv_font_conv](https://github.com/lvgl/lv_font_conv).

Source in `source/`:

- `materialdesignicons-webfont.ttf` — [@mdi/font](https://www.npmjs.com/package/@mdi/font) v7.x (Pictogrammers [Material Design Icons](https://pictogrammers.com/library/mdi/)), Apache 2.0 — same glyph set Home Assistant uses for `mdi:…` icons.

Subset (PUA codepoints):

| Home Assistant / MDI name   | Hex      |
|----------------------------|----------|
| lightbulb                  | `0xF0335` |
| fan                        | `0xF0210` |
| fan-off                    | `0xF081D` |
| fan-speed-1 … 3            | `0xF1472`–`0xF1474` |
| weather-sunny              | `0xF0599` |
| music-rest-quarter         | `0xF0F7A` |
| bed                        | `0xF02E3` |
| fire                       | `0xF0238` |
| fire-off                   | `0xF1722` |
| radiator-off               | `0xF0AD8` |
| thermometer                | `0xF050F` |
| thermometer-low            | `0xF10C3` |
| teddy-bear                 | `0xF18FB` |
| gate                       | `0xF0299` |
| gauge                      | `0xF029A` |
| desk (Study launcher)      | `0xF1239` |

UTF-8 C macros: `ui_home_assistant_icon_glyphs.h` (supplementary-plane glyphs use 4-byte UTF-8).

Regenerate from this directory:

```bash
npx lv_font_conv@1.5.2 --bpp 4 --size 56 --format lvgl --no-compress --no-prefilter --no-kerning \
  --font source/materialdesignicons-webfont.ttf \
  -r 0xF0335,0xF0210,0xF081D,0xF1472,0xF1473,0xF1474,0xF0599,0xF0F7A,0xF02E3,0xF0238,0xF0AD8,0xF050F,0xF1722,0xF10C3,0xF18FB,0xF029A,0xF0299,0xF1239 \
  -o ui_font_home_assistant_icons_56.c
```

Then replace the generated `#ifdef LV_LVGL_H_INCLUDE_SIMPLE` include block with a single `#include "lvgl.h"` (ESP-IDF does not put `lvgl/lvgl.h` on the include path when `LV_CONF_INCLUDE_SIMPLE` is set). Keep `UI_FONT_HOME_ASSISTANT_ICONS_56` guard naming consistent with the output.
