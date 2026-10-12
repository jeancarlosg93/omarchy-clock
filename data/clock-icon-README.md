# Clock icon source

The application mark is Pictogrammers Material Design Icons `clock-outline`, the same icon exposed by Nerd Fonts as `nf-md-clock_outline` (U+F0150). Omarchy's menu rows draw Nerd Font icons from the active shell font, with foreground/selected-text colors.

Source SVG: https://github.com/Templarian/MaterialDesign-SVG/blob/master/svg/clock-outline.svg
Nerd Fonts mapping: https://github.com/ryanoasis/nerd-fonts/blob/master/glyphnames.json
License: Apache 2.0, under the collection's [notice](clock-icon-LICENSE.txt) and [full license](clock-icon-APACHE-2.0.txt). The original path is unchanged; width/height and a theme-controlled fill were added. Application code remains MIT.

The app and tray render the bundled SVG with the active foreground. The bar uses the corresponding native Nerd Font glyph. Launcher icons are installed with the current foreground in their filename, and the desktop entry is updated atomically so the launcher does not reuse a cached previous-color image. A user-owned theme-set hook updates this icon even while the app is closed.
