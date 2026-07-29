# Wii U StandardShader Material Parameter Parity

## Objective

Carry authored unshadowed StandardShader scalar parameters from the platform material cook request through the Wii U runtime and into the existing reflected GX2 uniform blocks. This slice covers roughness, metallic, specular, and emissive color while preserving the already-proven base-color and diffuse-texture path.

The acceptance target is visual parity between DemoDisc `cube_test` on Windows and Wii U. Directional shadows, roughness textures, and emissive textures remain outside this slice.

## Current State

The Wii U shader compiler now produces a working generated `ForwardStandardShader`, including matching cross-stage varying names. The runtime uploads base color and emissive color from `WiiURuntimeMaterial`, but cooked materials currently use the shared `PlatformMaterialAsset`, which cannot represent StandardShader roughness, metallic, specular, or emissive fields.

As a result, `WiiUGx2Presenter` substitutes roughness `1.0`, metallic `0.0`, and specular `0.5`. `WiiURenderManager3D` also initializes emissive color to zero regardless of authored data. These substitutions prevent complete authored material parity even though the generated shader itself renders correctly.

## Chosen Architecture

Wii U will use a platform-owned, versioned StandardShader material payload analogous to the proven PS Vita compiled-shader material contract. The builder and native runtime will share an explicit binary layout identified by a stable magic prefix and version.

This approach is preferred over extending `PlatformMaterialAsset` because the extra fields belong to the Wii U generated-StandardShader runtime contract, not every fixed-pipeline platform. It also avoids modifying generated engine code. Reconstructing values from defaults at runtime is rejected because it discards authored material state.

The new payload applies to the generic `standard-shader` schema. The existing `wiiu-standard-textured` schema continues to serialize as `PlatformMaterialAsset`, and the native loader retains that format as a compatibility fallback.

## Cooked Material Contract

The platform-owned payload will contain:

- the material asset identifier;
- the fixed shared shader asset, vertex program, pixel program, and `ForwardStandard` variant identities;
- the optional cooked diffuse texture asset identity;
- base-color RGBA channels;
- roughness, metallic, and specular scalars;
- emissive-color RGBA channels, where alpha is emissive strength;
- lit and double-sided flags.

The serializer will write a little-endian payload with a four-byte Wii U material magic prefix and an explicit version. The native reader will decode integral and floating-point fields explicitly so the contract is independent of the Wii U CPU's native endianness. It will reject unsupported versions, invalid Boolean encodings, truncated data, and invalid required shader identities.

No generated C# or C++ asset type will be added or edited. Builder-owned C# types and handwritten native reader types remain the two sides of the contract.

## Authored Field Semantics

The cooker will recognize the shared StandardShader field identifiers `roughness`, `metallic`, `specular`, and `emissive-color`. Missing fields will use the same engine defaults as the Windows StandardShader path:

- roughness: `0.4`;
- metallic: `0.0`;
- specular: `0.5`;
- emissive color: `#FFFFFF00`.

Scalar text will be parsed with invariant culture and normalized to the zero-to-one range, matching the shared StandardMaterial constant-buffer helpers. Emissive color will accept `#RRGGBB` and `#RRGGBBAA`; the six-digit form receives fully opaque alpha, matching existing color parsing. Invalid authored values will fail material cooking rather than being replaced silently.

The existing base-color, diffuse-texture, lighting, and double-sided semantics remain unchanged. This design does not introduce roughness or emissive texture fields into the runtime binding path.

## Runtime Loading

For cooked path-based material loading, `WiiURenderManager3D` will first ask the new reader whether the staged file uses the platform-owned StandardShader payload. A matching payload will be validated and converted directly into `WiiURuntimeMaterial`. A nonmatching magic prefix will reopen the content and use the existing `AssetSerializer`/`PlatformMaterialAsset` path.

Both the direct file path and `IContentStreamSource` overloads must support this detection without assuming a seekable stream. Each fallback path will obtain a fresh stream after format probing.

The existing `BuildMaterialFromCooked(PlatformMaterialAsset*)` generated-core override remains supported and constructs the Windows-compatible StandardShader defaults for fields absent from that legacy contract.

## Runtime Material and GX2 Upload

`WiiURuntimeMaterial` will own roughness, metallic, and specular values alongside its existing base and emissive colors. Its constructor will initialize all four StandardShader parameters to the shared defaults, and explicit setters/getters will preserve values decoded from the cooked payload.

`WiiUGx2Presenter` will populate `RoughnessBuffer`, `MetallicBuffer`, `SpecularBuffer`, and `EmissiveColorBuffer` from the draw command's runtime material. Existing reflected block lookup, size validation, cache invalidation, and GX2 binding remain unchanged. The solid-white fallback textures remain bound for roughness and emissive sampling, causing the scalar/color values to control the untextured result exactly as the shared HLSL specifies.

No shader source, generated Cafe shader artifact, varying mapping, lighting buffer, or shadow buffer behavior changes in this slice.

## Validation

Automated tests will verify:

- the platform-owned serializer round-trips every field;
- invalid magic, version, truncation, Boolean values, and required identities fail explicitly;
- `standard-shader` cooking emits the new payload and the expected shader dependency;
- authored scalar and emissive fields survive cooking;
- absent fields produce the Windows-compatible defaults;
- invalid numeric or color fields fail cooking;
- the legacy `wiiu-standard-textured` schema retains its current generic payload;
- runtime source carries the decoded parameters into `WiiURuntimeMaterial`;
- presenter source uploads runtime values rather than hardcoded scalar constants.

The smallest relevant builder test set will run first, followed by the complete Wii U builder test suite before committing implementation.

After an authoritative Wii U package build, the exact DemoDisc Cemu shader-cache files will be cleared and Cemu will be launched. Helena will manually navigate to `cube_test` and compare its authored material response with Windows. Automated navigation, UI typing, OCR, and screenshots will not be used unless Helena explicitly requests them.

## Scope Boundaries

This change does not modify directional-shadow rendering, shadow material flags, shadow-map generation, shader compilation, cross-stage varying naming, roughness textures, emissive textures, DemoDisc scene sources, or any other platform's material format. Further texture support and the return to the Directional Shadow scene require separate validated slices after unshadowed scalar parity is proven.
