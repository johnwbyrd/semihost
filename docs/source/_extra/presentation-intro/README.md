# ZBC Introductory Presentation

A self-contained [reveal.js](https://revealjs.com/) deck introducing the Zero
Board Computer and semihosting: where the name comes from, the bring-up problem,
what semihosting is, how ZBC reframes it as a 32-byte memory-mapped device, width
neutrality, ARM compatibility, the multi-transport story (RIFF device, virtio on
stock QEMU, native trap), and how to try it. Slides are kept deliberately sparse
— one idea each — to avoid overflow on a projector.

## Running it

`index.html` loads reveal.js from a CDN, so the simplest path is to open it over
HTTP (opening the `file://` URL directly works too, but a local server avoids
browser restrictions):

```bash
cd docs/source/_extra/presentation-intro
python3 -m http.server 8000
# then open http://localhost:8000/
```

The deck is also published as part of the docs site at
`<docs-base-url>/presentation-intro/` — Sphinx copies it through verbatim
via `html_extra_path` in `docs/source/conf.py`.

## Presenting

- **Speaker notes** — press `S` to open the notes/preview window. Every slide
  has notes that expand on the talking point.
- **Navigation** — arrow keys, or `Space` to advance.
- **Overview** — press `Esc` to see all slides at once.
- **Slide numbers** are shown bottom-right (`current / total`).

## Editing

Content is plain HTML in `index.html` — one `<section>` per slide. Incremental
reveals use `class="fragment"`. Notes live in `<aside class="notes">`. Styling
(accent colors, cards, columns) is in the `<style>` block at the top.

The narrative tracks `docs/source/introduction.rst`, the spec
(`docs/source/specification.rst`), and the transport story
(`docs/source/qemu-transports.rst`); keep the register map, opcode numbers,
virtio-mmio windows, and the width-neutrality framing in sync with those
normative sources if they change.

Note: the repo does not formally define *why* it's a "Zero Board Computer." The
deck reads it as a play on **Single Board Computer** (one board → zero boards);
the maintainer may want to confirm or reword that framing.
