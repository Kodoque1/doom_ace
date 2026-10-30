# docs/: what GitHub Pages serves

| | |
|---|---|
| `index.html` | the Doomception article, one self-contained page. **Built, do not edit it here.** |
| `doomception.mp4` | the recording the article opens with |
| `og.png` | the preview image a shared link shows |
| `challenge/` | the challenge page and the kit's programs compiled to WebAssembly. **Built** by `sandbox/web/build.sh` |

## Changing the article

Edit the sources in [`../nested/docs/doomception-src/`](../nested/docs/doomception-src/),
then rebuild:

    python3 nested/docs/doomception-src/build.py

- `body.html`: the text and the page's structure
- `v2.css`: the article's styles
- `v2.js`: the hero ring, the 3D scene and its legend
- `anat.js`: the Anatomy figures
- `drawview.js`: the procedural game view the scenes draw

The numbers on the page come from `nested/docs/mtx-run.json`.

## Changing the challenge page

Edit `sandbox/web/page.html` (or the kit itself), then run `sandbox/web/build.sh`.
It needs clang with the wasm32 target.
