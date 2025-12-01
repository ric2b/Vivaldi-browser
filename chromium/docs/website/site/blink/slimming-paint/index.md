---
breadcrumbs:
- - /blink
  - Blink (Rendering Engine)
page_name: slimming-paint
title: Slimming Paint (a.k.a. Redesigning Painting and Compositing)
---

Slimming Paint was a project to re-implement the Blink&lt;-&gt;cc picture recording API to work in terms of a global display list rather than a tree of cc::Layers (~aka GraphicsLayer in Blink terminology). It was completed in several phases from 2015 to 2021.

The project simplified compositing (following launch, 22,000 lines of c++ were removed),  fixed correctness ([fundamental compositing bug](https://crbug.com/40364303)), improved performance (total chrome CPU usage -1.3%, 3.5%+ improvement to 99th percentile scroll update, 2.2%+ improvement to 95th percentile input delay), and led to a more flexible architecture which has been built upon by many other projects (e.g., [HitTestOpaqueness](https://crbug.com/40062957), [RasterInducingScroll](https://docs.google.com/document/d/1ZaQQtcDC0EAkldXJp0h3sBvV9wyR4le048Al1RtIUZw/preview)).

## Phases

SlimmingPaintV1 - paint using display items ([Status: launched in
M45](https://groups.google.com/a/chromium.org/forum/#!searchin/blink-dev/slimming$20paint/blink-dev/qq4NEaqSrKM/OKiNzm3PkSkJ))

SlimmingPaintInvalidation - rewrite of paint invalidation using display items,
property trees introduced in blink ([Status: launched in
M58](https://groups.google.com/a/chromium.org/forum/#!searchin/blink-dev/slimming$20paint/blink-dev/YXcuTl6PbDk/7jAte1CDBAAJ))

SlimmingPaintV175 - uses property trees for painting in blink, introduces paint
chunks, uses paint chunks for raster invalidation ([Status: launched in
M67](https://bugs.chromium.org/p/chromium/issues/detail?id=771643))

[BlinkGenPropertyTrees](https://docs.google.com/document/d/17GKr2uIH2O5GthdTyvJpv1qZjoHYoLgrzvCkbCHoID4/view#)
- sends a layer list instead of a tree, and generates final property trees in
blink instead of re-generating them in cc ([Status: launched in
M75](https://crbug.com/836884))

[CompositeAfterPaint](https://docs.google.com/document/d/114ie7KJY3e850ZmGh4YfNq8Vq10jGrunZJpaG6trWsQ/edit)
- compositing decisions made after paint ([Status: launched in M94](https://crbug.com/40081744))

## Presentations

[BlinkOn 3.0
Presentation](https://docs.google.com/presentation/d/1zpGlx75eTNILTGf3s_F6cQP03OGaN2-HACsZwEobMqY/edit?usp=sharing),
[video](https://youtube.com/watch?v=5Xv2A7aqJ9Y) (start here to find out
more about the project)

[BlinkOn 4
presentation](https://docs.google.com/presentation/d/17k62tf1zc5opvIfhCXMiL4UdI9UGvtCJbUEKMPlWZDY/edit)

[Blink Property
Trees](https://docs.google.com/presentation/d/1ak7YVrJITGXxqQ7tyRbwOuXB1dsLJlfpgC4wP7lykeo)
(also reviews the Composite-After-Paint (formerly "SPV2") design)

[BlinkOn 9
presentation](https://docs.google.com/presentation/d/1Iko1oIYb-VHwOOFU3rBPUcOO_9lAd3NutYluATgzV_0/view#slide=id.g36f1b50c08_0_1902)
covering current compositing architecture

### Core team members

Chris Harrelson (chrishtr@), overall TL

Mason Freed (masonfreed@) Blink

Philip Rogers (pdr@) Blink

Stephen Chenney (schenney@) Blink

Xianzhu Wang (wangxianzhu@) Blink

Adrienne Walker (enne@) cc

Robert Flack (flackr@) animations

David Bokan (bokan@) viewports

#### Close relatives

Fredrik Söderquist (fs@opera.com) Blink

Erik Dahlstrom (ed@opera.com) Blink

Florin Malita (fmalita@) Blink / Skia

Mike Klein (mtklein@) Skia

Mike Reed (reed@) Skia

A number of other people are involved at least tangentially for design
discussions and related projects.

## Resources and Design Docs

The [BlinkNG](https://developer.chrome.com/docs/chromium/blinkng) blogpost has a high-level overview of how slimming paint fits into the overall rendering architecture.

[core/paint/README.md](https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/core/paint/README.md)

[platform/graphics/paint/README.md](https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/platform/graphics/paint/README.md)

[Slimming paint
invalidation](https://docs.google.com/document/d/1M669yu7nsF9Wrkm7nQFi3Pp2r-QmCMqm4K7fPPo-doA)

[Representation and implementation of display lists in
Blink](https://docs.google.com/document/d/1fWvFIY41BJHtB4qBHw3_IZYqScurID4KmE2_a6Be0J4/preview?usp=sharing)

[Layerization based on display
lists](https://docs.google.com/a/google.com/document/d/1L6vb9JEPFoyt6eNjVla2AbzSUTGyQT93tQKgE3f1EMc/preview)

[Blink paintlist update algorithm
details](https://docs.google.com/document/d/1bvEdFo9avr11S-2k1-gT1opdYWnPWga68CK3MdoYV7k/preview?usp=sharing)

[Bounding Rectangle Strategy for Slimming
Paint](https://docs.google.com/a/chromium.org/document/d/12G3rfM3EkLYDCRcO1EpEObfeoU24Sqof9S0sPHgELU4/preview?usp=sharing)

[Slimming Paint for UI
Compositor](https://docs.google.com/a/chromium.org/document/d/1Oxa3E-ymCqY2-7AlMrL1GEqAtyFE__0PRzhg9EEZt7Y/preview)

[Debugging blink objects](https://docs.google.com/document/d/1vgQY11pxRQUDAufxSsc2xKyQCKGPftZ5wZnjY2El4w8/preview) has information about inspecting the datastructures introduced by slimming paint.

Some out of date/historical docs are
[here](/blink/slimming-paint/historical-documents).
