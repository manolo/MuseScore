# Brand

The identity of the fork's builds, kept apart from the integration lines so that changing a colour never touches a line's history.

## The icon

A plectrum seen head on, with the rosette of a Spanish soundhole cut through it: the hole, the ring, and twelve spokes, one per string of a bandurria, six courses doubled.

`make-icon.sh` generates every format from the geometry written in it. The generated files sit beside it and are committed, so a build never needs ImageMagick; the script exists to change the design, not as a build step.

```sh
fork/brand/make-icon.sh          # rewrites the files in place
open fork/brand/preview.png      # the sizes that matter, on light and dark
```

| File | For |
|---|---|
| `PlectroScore.icns` | macOS bundle |
| `PlectroScore.ico` | Windows, six layers from 256 down to 16 |
| `plectroscore-<n>.png` | Linux, 16 to 1024 |
| `preview.png` | 128, 64, 32 and 16 on both grounds |

## Two things learnt drawing it

**No SVG master.** The obvious route is an SVG rasterised on demand, and it was abandoned: the rsvg build ImageMagick delegates to here silently drops `<mask>` and mishandles `rotate()` about a point, so the icon came out blank, then black, then displaced. Drawing straight with ImageMagick is a format nobody has to guess about.

**The first silhouette was wrong.** Narrow, with a long tapering tip, it read unmistakably as a map pin at small sizes. A real 351 pick is nearly as wide as it is tall, with broad shoulders and a blunt tip, which is what the script draws now. The rosette is heavier than a luthier would cut it so that it survives being shrunk; below about 24 pixels it closes up anyway, and only the silhouette and the colour identify the app, which is all a Dock icon needs.
