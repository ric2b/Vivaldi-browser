# AVIF-info

**libavifinfo** is a standalone library that can be used to extract the width,
height, bit depth, number of channels and other metadata from an AVIF payload.

See `avifinfo.h` for details on the API and `avifinfo.c` for the implementation.
See `tests/avifinfo_demo.cc` for API usage examples.

## How to use

**libavifinfo** can be used when only a few AVIF features are needed and when
linking to or including [libavif](https://github.com/AOMediaCodec/libavif) is
not an option. For decoding an image or extracting more features, please rely on
[libavif](https://github.com/AOMediaCodec/libavif).

Note: The C implementation `AvifInfoGetFeatures()` is designed to return the
same `avifImage` field values as
[`avifDecoderParse()`](https://github.com/AOMediaCodec/libavif/blob/e34204f5370509c72b3b2f065e5ebb2767cbbd48/include/avif/avif.h#L1049).
However **libavifinfo** is often more permissive and may return features of
images considered invalid by **libavif**.

## C implementation

### Coding style

[Google C/C++ Style Guide](https://google.github.io/styleguide/cppguide.html) is
used in this project.

### Build

`avifinfo.c` is written in C. To build from this directory:

```sh
cmake -S . -B build && \
cmake --build build --config Release
```

### Test

GoogleTest is required for the C++ tests (e.g. `sudo apt install libgtest-dev`).

```sh
cmake -S . -B build -DAVIFINFO_BUILD_TESTS=ON && \
cmake --build build --config Debug && \
ctest --test-dir build build
```

## PHP implementation

The PHP implementation of libavifinfo is a subset of the C API.

`libavifinfo` was [implemented](https://github.com/php/php-src/pull/7711) into
**php-src** natively and is available through `getimagesize()` at head. If it is
not available in the PHP release version you use, you can fallback to
`avifinfo.php` instead.

See `avifinfo_test.php` for a usage example.

### PHP test

```sh
cd tests && \
php avifinfo_test.php
```

## Development

### Submitting patches {#submitting-patches}

If you would like to contribute to **libavifinfo**, please follow the steps for
**libaom** at https://aomedia.googlesource.com/aom/#submitting-patches. Summary:

- Log in at https://aomedia.googlesource.com
- Sign in at https://aomedia-review.googlesource.com
- Execute the contributor agreement if necessary, see http://aomedia.org/license
- Test all implementations as described in this document
- Copy the commit message hook:
  `cd libavifinfo && curl -Lo .git/hooks/commit-msg https://chromium-review.googlesource.com/tools/hooks/commit-msg && chmod u+x .git/hooks/commit-msg`
- Create a commit
- `git push https://aomedia.googlesource.com/libavifinfo/ HEAD:refs/for/main`
- Request a review from a project maintainer

## Known users of libavifinfo

- [Chromium](https://source.chromium.org/chromium/chromium/src/+/main:third_party/libavifinfo)
  uses the C impl to extract the gain map item for HDR tone mapping.
- [PHP-src](https://github.com/php/php-src/tree/master/ext/standard/libavifinfo)
  uses the C impl to parse image features without relying on libavif or libheif.
- [WordPress](https://github.com/WordPress/wordpress-develop/blob/trunk/src/wp-includes/class-avif-info.php)
  uses the PHP impl to support AVIF images with older versions of PHP.

## Bug reports

Bug reports can be filed in the Alliance for Open Media
[issue tracker](https://bugs.chromium.org/p/aomedia/issues/list).
