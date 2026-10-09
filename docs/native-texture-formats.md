# Wii U native texture formats

The catalog covers all 65 valid `GX2SurfaceFormat` selections in the installed
WUT SDK. `INVALID` is rejected. There are 39 image-cookable selections and
26 raw-only selections for integer, depth/stencil and NV12 shader data.
Stable setting identifiers use `WiiU.<SDK format suffix>`.

Image and font-atlas defaults remain `Rgba32/A8` with `MaxResolution=0`.
The builder exposes the native image formats alongside the existing generic
formats. Generic indexed formats retain the selected quantization strategy;
GX2 native surface formats do not contain palette indices.

## Serialized contract

`TextureAssetColorFormat.WiiUNative` (21) uses `Colors` for a WGT1 descriptor
followed by compact native texels. `PaletteColors` must be empty. The descriptor
contains sixteen little-endian uint32 words, independently of the outer
asset container's byte order:

| Offset | Value |
| --- | --- |
| 0 | Magic `0x31544757` (`WGT1`) |
| 4 | Version 1 |
| 8 | Exact `GX2SurfaceFormat` |
| 12 | Compact transport tile mode, `LINEAR_SPECIAL` (16) |
| 16, 20 | Logical width and height |
| 24, 28 | Storage width and height |
| 32 | Scanline or BC block-row byte length |
| 36 | Native texel length, excluding this header |
| 40 | Canonical sampled RGBA component map |
| 44 | Native texture endian swap mode, zero |
| 48 | Texture asset alpha precision |
| 52 | Reserved flags, zero |
| 56 | Compact NV12 UV plane byte offset, otherwise zero |
| 60 | Reserved, zero |

Single-level 2D dimensions are limited to 1..8192. BC storage completes its
4x4 blocks; NV12 storage completes even width and height. Numeric components
and BC blocks use native little-endian bytes even though the Wii U CPU and
outer packaged asset metadata are big endian. Raw import validates exact
byte lengths and preserves those native bytes.

Packed channels follow the physical GX2 format and explicit component map.
The reverse A1/B5/G5/R5 and A2/B10/G10/R10 layouts retain low-bit alpha and
reverse component selectors. The packed signed two-bit alpha field can
represent only zero and one in normalized image alpha, so it exposes binary
alpha rather than four alpha levels.

sRGB cooking applies the RGB transfer function before encoding, and preview
applies the inverse sampler transfer function. Alpha remains independent.
This preserves the normalized source samples when switching from an ordinary
UNORM texture to sRGB sampling. The implementation uses fixed byte lookup
tables; BC compression introduces its usual color approximation.

`WiiUNativeTextureLayout.TexelLength` and `GetTransportByteLength` measure the
compact packaged texels. `GetVramByteLength` measures the single-level
`LINEAR_ALIGNED` destination using its pitch padding and NV12 plane placement;
it excludes the WGT1 descriptor and allocator bookkeeping.

## Runtime contract

Compact `LINEAR_SPECIAL` data is transport storage. The runtime creates an
owned, SDK-calculated `LINEAR_ALIGNED` GX2 surface and copies native scanlines
or BC block rows into its actual pitch. It preserves the format and component
map in 2D and 3D without expanding compressed native uploads to RGBA32.

Integer textures use point sampling and require a compatible native shader.
Depth/stencil textures also require the corresponding shader contract.
NV12 exposes borrowed Y and UV views over one owned allocation, with logical
dimensions retained separately from even storage extents. A planar shader
must bind and convert both planes; ordinary UI and standard shaders do not
automatically turn raw native data into RGBA images.

RGBA region updates support legacy RGBA32 and native `UNORM_R8_G8_B8_A8`.
Other native formats reject updates before a fence or mutation. Legacy font
coverage bytes remain intact; cooked native alpha honors its selected policy.
Updates wait for GX2 rendering before writing and invalidate CPU/texture
caches afterward. Texture records retain stable addresses, and deferred
texture/material releases wait for GPU completion before destroying surfaces.

The builder prepares and validates all declared texture/font work items before
publishing the package or invoking the native build. Explicit external font
atlases retain their cooked bytes and paths.

## References and validation

Primary API contracts are the [WUT GX2 enum](https://github.com/devkitPro/wut/blob/master/include/gx2/enum.h),
[surface API](https://github.com/devkitPro/wut/blob/master/include/gx2/surface.h)
and [texture API](https://github.com/devkitPro/wut/blob/master/include/gx2/texture.h).
Packing and pitch checks also use the pinned
[Cemu texture implementation](https://github.com/cemu-project/Cemu/blob/fced55a9dee09292587d509c30904494d4f6e8f2/src/Cafe/HW/Latte/Core/LatteTextureLoader.h)
and [address library](https://github.com/cemu-project/Cemu/blob/fced55a9dee09292587d509c30904494d4f6e8f2/src/Cafe/HW/Latte/LatteAddrLib/LatteAddrLib.cpp),
alongside the [Decaf surface implementation](https://github.com/decaf-emu/decaf-emu/blob/master/src/libdecaf/src/cafe/libraries/gx2/gx2_surface.cpp).

SDK headers, actual cooker fixtures, tests, generated Core and compiler logs
use the fixed directory `C:/dev/helworks/builds/helengine-wiiu/texture-formats`.
SDK compilation and host tests are separate from visual verification.
This implementation task does not build a complete RPX/WUHB or launch Cemu
or Wii U hardware.
