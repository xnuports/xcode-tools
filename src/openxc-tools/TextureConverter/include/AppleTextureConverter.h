//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// AppleTextureConverter.h
//
//-------------------------------------------------------------------------------------------------------------------------------------------------------------

// This API will evolve over time, in ways that may break existing code.
// As a static library it will not break your usage, but you may need to change your code if you adopt a later version.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define atcMax(a, b) ((a > b) ? a : b)
#define atcMin(a, b) ((a < b) ? a : b)

#define ATC_API_VER 2 // This will increment for each API update

// Errors returned by ATC functions. Check the ATC_MessageCallback for more details.
typedef enum
{
    atcErrorNone,                   // Success!
    atcErrorUnknown,                // Unknown error.
    atcErrorInvalidContext,         // The context struct is invalid.
    atcErrorInvalidOptions,         // The options struct is invalid.
    atcErrorInvalidSourceTexture,   // The source texture or texture file is invalid.
    atcErrorInvalidDestTexture,     // The destination texture or texture file is invalid.
    atcErrorMemory                  // Error allocating memory.
} ATC_Error;

// Alpha mode for compression.
typedef enum
{
    atcAlphaIgnore,         // Ignore alpha.
    atcAlphaPremultiply,    // Pre-multiply color by alpha.
    atcAlphaPreserve        // Preserve alpha.
} ATC_AlphaMode;

// Channel weightings to use when compressing.
typedef enum
{
    atcWeightingLinear,     // Use linear weighting (1.0, 1.0, 1.0, 1.0).
    atcWeightingPerceptual  // Use perceptual weighting (0.299, 0.587, 0.114, 1.0).
} ATC_ChannelWeighting;

// Filtering method to use when generating mip-maps.
typedef enum
{
    atcMipmapFilterBox,         // Box filter.
    atcMipmapFilterTriangle,    // Triangle filter.
    atcMipmapFilterKaiser       // Kaiser filter.
} ATC_MipmapFilter;

// Filtering method to use when resizing textures.
typedef enum
{
    atcResizeFilterBox,         // Box filter.
    atcResizeFilterTriangle,    // Triangle filter.
    atcResizeFilterKaiser,      // Kaiser filter.
    atcResizeFilterMitchell     // Mitchell filter.
} ATC_ResizeFilter;

// Rounding mode to use when resizing textures.
typedef enum
{
    atcRoundModeNone,                   // Don't round.
    atcRoundModeNextPowerOfTwo,         // Resize to next (higher) power of two.
    atcRoundModeNearestPowerOfTwo,      // Resize to nearest power of two.
    atcRoundModePreviousPowerOfTwo,     // Resize to previous (lower) power of two.
    atcRoundModeNextMultipleOfFour,     // Resize to next (higher) power of four.
    atcRoundModeNearestMultipleOfFour,  // Resize to nearest power of four.
    atcRoundModePreviousMultipleOfFour  // Resize to previous (lower) power of four.
} ATC_RoundMode;

// Wrap mode to use when resizing textures or generating mip-maps.
typedef enum
{
    atcWrapModeClamp,   // Clamp to edge.
    atcWrapModeRepeat,  // Repeat.
    atcWrapModeMirror   // Mirror.
} ATC_WrapMode;

// Quality level to use when compressing. Higher quality levels entails slower compression.
typedef enum
{
    atcQualityFastest,      // Fastest/lowest-quality compression. Best avoided for release builds.
    atcQualityNormal,       // Fast/reasonable-quality compression.
    atcQualityProduction,   // Slower/better-quality compression.
    atcQualityHighest       // Slowest/higher-quality compression. Best avoided for debug builds.
} ATC_Quality;

// Format to use when writing texture files.
typedef enum
{
    atcFileFormatAuto,      // Auto-select the file format based on the filename extension.
    atcFileFormatDDS,       // DDS file format.
    atcFileFormatHeader,    // C/C++ Header
    atcFileFormatKTX,       // KTX file format.
    atcFileFormatKTX2,      // KTX2 file format.
    atcFileFormatEXR,       // OpenEXR file format.
} ATC_FileFormat;

// Alpha punch-through mode. Used for PVRTC compression only.
typedef enum
{
    atcPVRPunchThroughAllowed,  // Punch-through is allowed.
    atcPVRPunchThroughForced,   // Punch-through is forced-on.
    atcPVRPunchThroughUnused    // Punch-through is forced-off.
} ATC_PVRPunchThrough;

typedef enum
{
    atcFormatUnknown,               // Undefined format

// Uncompressed formats. These are the source formats for compression operations and the destination formats for decompression operations.

    atcFormatR8Unorm,               // 1-channel, 8 bits-per-pixel, unsigned
    atcFormatR16F16,                // 1-channel, 16 bits-per-pixel, half-float
    atcFormatR32F32,                // 1-channel, 32 bits-per-pixel, float

    atcFormatRg8Unorm,              // 2-channel, 16 bits-per-pixel, unsigned
    atcFormatRg16F16,               // 2-channel, 32 bits-per-pixel, half-float
    atcFormatRg32F32,               // 2-channel, 64 bits-per-pixel, float

    atcFormatRgb8Unorm,             // RGB, 24 bits-per-pixel, unsigned
    atcFormatRgb16F16,              // RGB, 48 bits-per-pixel, half-float
    atcFormatRgb32F32,              // RGB, 96 bits-per-pixel, float

    atcFormatRgba8Unorm,            // RGBA, 32 bits-per-pixel, unsigned
    atcFormatRgba16F16,             // RGBA, 64 bits-per-pixel, half-float
    atcFormatRgba32F32,             // RGBA, 128 bits-per-pixel, float

    atcFormatBgra8Unorm,            // BGRA, 32 bits-per-pixel, unsigned

// Compressed formats. These are the destination formats for compression operations and the
// sourceFile formats for decompression operations.

    // BC (aka DXTC) compression formats.
    atcFormatBc1Unorm,              // BC1 (DXT1) - 4x4, 4 bits-per-pixel - RGB with optional 1 bit alpha
    atcFormatBc2Unorm,              // BC2 (DXT2/DXT3) - 4x4, 8 bits-per-pixel - RGBA, explicit alpha
    atcFormatBc3Unorm,              // BC3 (DXT4/DXT5) - 4x4, 8 bits-per-pixel - RGBA, interpolated alpha
    atcFormatBc4Snorm,              // BC4 - 4x4, 4 bits-per-pixel, 1-channel, signed
    atcFormatBc4Unorm,              // BC4 - 4x4, 4 bits-per-pixel, 1-channel, unsigned
    atcFormatBc5Snorm,              // BC5 - 4x4, 8 bits-per-pixel, 1-channel, signed
    atcFormatBc5Unorm,              // BC5 - 4x4, 8 bits-per-pixel, 1-channel, unsigned
    atcFormatBc6Sf16,               // BC6S - RGB 4x4, 8 bits-per-pixel HDR, signed
    atcFormatBc6Uf16,               // BC6U - RGB 4x4, 8 bits-per-pixel HDR, unsigned
    atcFormatBc7Unorm,              // BC7 - RGB/RGBA 4x4, 8 bits-per-pixel LDR

    // ASTC compression formats. All ASTC formats take 16 bytes per block. Smaller block dimensions provide higher quality for lower compression-rates,
    // higher block dimensions provide lower quality for higher compression-rates. HDR variants are supported on A13 & later GPUs.
    atcFormatAstc4x4Unorm,          // ASTC 4x4, LDR
    atcFormatAstc4x4F16,            // ASTC 4x4, HDR
    atcFormatAstc5x4Unorm,          // ASTC 5x4, LDR
    atcFormatAstc5x4F16,            // ASTC 5x4, HDR
    atcFormatAstc5x5Unorm,          // ASTC 5x5, LDR
    atcFormatAstc5x5F16,            // ASTC 5x5, HDR
    atcFormatAstc6x5Unorm,          // ASTC 6x5, LDR
    atcFormatAstc6x5F16,            // ASTC 6x5, HDR
    atcFormatAstc6x6Unorm,          // ASTC 6x6, LDR
    atcFormatAstc6x6F16,            // ASTC 6x6, HDR
    atcFormatAstc8x5Unorm,          // ASTC 8x5, LDR
    atcFormatAstc8x5F16,            // ASTC 8x5, HDR
    atcFormatAstc8x6Unorm,          // ASTC 8x6, LDR
    atcFormatAstc8x6F16,            // ASTC 8x6, HDR
    atcFormatAstc8x8Unorm,          // ASTC 8x8, LDR
    atcFormatAstc8x8F16,            // ASTC 8x8, HDR
    atcFormatAstc10x5Unorm,         // ASTC 10x5, LDR
    atcFormatAstc10x5F16,           // ASTC 10x5, HDR
    atcFormatAstc10x6Unorm,         // ASTC 10x6, LDR
    atcFormatAstc10x6F16,           // ASTC 10x6, HDR
    atcFormatAstc10x8Unorm,         // ASTC 10x8, LDR
    atcFormatAstc10x8F16,           // ASTC 10x8, HDR
    atcFormatAstc10x10Unorm,        // ASTC 10x10, LDR
    atcFormatAstc10x10F16,          // ASTC 10x10, HDR
    atcFormatAstc12x10Unorm,        // ASTC 12x10, LDR
    atcFormatAstc12x10F16,          // ASTC 12x10, HDR
    atcFormatAstc12x12Unorm,        // ASTC 12x12, LDR
    atcFormatAstc12x12F16,          // ASTC 12x12, HDR

    // ETC2/EAC compression formats.
    atcFormatEacR11Snorm,           // EAC 1-channel, signed
    atcFormatEacR11Unorm,           // EAC 1-channel, unsigned
    atcFormatEacRg11Snorm,          // EAC 2-channel, signed
    atcFormatEacRg11Unorm,          // EAC 2-channel, unsigned
    atcFormatEacRgba8Unorm,         // EAC RGBA
    atcFormatEtc2Rgb8Unorm,         // ETC2 RGB
    atcFormatEtc2Rgb8A1Unorm,       // ETC2 RGB with 1-bit alpha

    // PVRTC compression formats.
    atcFormatPvrtcRgb2BppUnorm,     // PVRTC RGB 2 bits-per-pixel
    atcFormatPvrtcRgb4BppUnorm,     // PVRTC RGB 4 bits-per-pixel
    atcFormatPvrtcRgba2BppUnorm,    // PVRTC RGBA 2 bits-per-pixel
    atcFormatPvrtcRgba4BppUnorm     // PVRTC RGBA 4 bits-per-pixel
} ATC_Format;

// Supported color gamuts
typedef enum {
    atcColorGamutUnknown,
    atcColorGamutNone,
    atcColorGamutDisplayP3,
    atcColorGamutSRGB,
} ATC_ColorGamut;

// Available texture compressors. Use ATC_QueryCompressorInfo to query compressor capabilities.
typedef enum
{
    atcCompressorAuto,      // Auto compressor selects which compressor to use based on the format & quality level. Use this compressor unless you require a specific one.
    atcCompressorArm,       // Arm astc-encoder 4.8 (https://github.com/ARM-software/astc-encoder/).
    atcCompressorETC2COMP,  // Google etc2comp (https://github.com/google/etc2comp).
    atcCompressorISPC,      // Intel ISPC Texture Compressor (https://github.com/GameTechDev/ISPCTextureCompressor).
    atcCompressorNVTT,      // NVTT (https://github.com/castano/nvidia-texture-tools).
    atcCompressorPVRTC,     // PVRTC compressor. Available on macOS only. PVRTC support in Metal is now deprecated. Usage of ASTC/ETC2/BC formats is recommended instead.
    atcCompressorSTB,       // STB DXT compressor (https://github.com/nothings/stb).
    atcCompressorRaw        // No compression, for raw formats such as atcFormatRgba8Unorm.
} ATC_Compressor;

// Available texture decompressors.
typedef enum
{
    atcDecompressorAuto,    // Auto decompressor selects which decompressor to use based on the format. Use this decompressor unless you require a specific one.
    atcDecompressorArm,     // Arm astc-encoder 3.7 (https://github.com/ARM-software/astc-encoder/).
    atcDecompressorGPU,     // GPU texture-sampling based decompressor, using Metal on macOS & D3D11 on Windows.
    atcDecompressorNVTT,    // NVTT (https://github.com/castano/nvidia-texture-tools).
    atcDecompressorPVRTC,   // PVRTC compressor. Available on macOS only. PVRTC support in Metal is now deprecated. Usage of ASTC/ETC2/BC formats is recommended instead.
    atcDecompressorRaw      // No decompression, for raw formats such as atcFormatRgba8Unorm.
} ATC_Decompressor;

// Type of texture.
typedef enum
{
    atcTextureType2D,
    atcTextureTypeCube,
    atcTextureType3D,
    atcTextureType2DArray,
} ATC_TextureType;

// Texture maximum size definitions
#define atcMaxMipMaps   15              // The maxinum supported number of mip-levels, including the top mip-level.
#define atcMaxFaces     6               // Maxinum number of faces for cubemaps.
extern const uint32_t atcMaxExtent;     // The maximum height and width of a texture. Calculated to fit atcMaxMipMaps.
#define atcMaxDepth     512             // The maximum depth of a texture for 3D textures.
#define atcMaxElements  512             // The maximum number of array elements for 2D array textures.
#define atcMaxSurfaces  512

// Per-surface (mip-level-face) data.
typedef struct ATC_Surface
{
    uint32_t        width;      // (Optional) Width of the surface. If zero this will be calculated based on ATC_Texture & mip-level.
    uint32_t        height;     // (Optional) Height of the surface. If zero this will be calculated based on ATC_Texture & mip-level.
    uint32_t        rowBytes;   // (Optional) Width of the surface rows in bytes. If zero this will be calculated based on ATC_Texture & mip-level. Should be zero for compressed formats.
    void*           data;       // Pointer to the surface data.
    uint32_t        size;       // Size of the surface data in bytes.
} ATC_Surface;

// Top-level texture.
typedef struct ATC_Texture
{
    uint32_t        width;                                  // Width of the texture. Must be greater than zero & not greater than atcMaxExtent.
    uint32_t        height;                                 // Height of the texture. Must be greater than zero & not greater than atcMaxExtent.
    uint32_t        numMipLevels;                           // Number of mip-levels. Must not exceed atcMaxMipMaps.
    union {
        uint32_t	depth;                                  // Depth of the texture for 3D textures. Must be greater than zero & not greater than atcMaxDepth.
        uint32_t	elements;                               // Number of elements for 2D texture arrays. Must be greater than zero & not greater than atcMaxElements.
    };
    ATC_TextureType type;                                   // Type of texture (2D, Cube, etc).
    ATC_Format      format;                                 // Format of the texture.
    ATC_ColorGamut  colorGamut;                             // Color gamut of the texture, sRGB or DisplayP3 if it is a color. Must be atcColorGamutNone for normal maps.
    bool            isNormalMap;                            // Texture is a normal map.
    uint32_t		surfaceCount;                           // Total number of surfaces.
    ATC_Surface*	surfaces;                               // Pointer to an array of ATC_Surface structures. Use the ATC_GetSurface function to access surfaces.
} ATC_Texture;

// Options to define how to perform operations.
typedef struct ATC_Options
{
    ATC_Compressor          compressor;             // The compressor to use while compressing.
    ATC_Decompressor        decompressor;           // The decompressor to use while decompressing. Will also be used compare operations if decompression
    												// is required.

    ATC_Format              format;                 // Texture format for the destination texture.
    ATC_Quality             quality;                // Quality level to use while compressing.

    ATC_AlphaMode           alphaMode;              // Alpha mode for compression.
    bool                    useAlphaToCoverage;     // Use alpha to coverage when compressing.
    float                   alphaReference;         // Alpha reference for useAlphaToCoverage.
    bool                    useAlphaWeight;         // Use alpha weight when compressing.

    bool                    isGammaInSrgb;          // Source texture is sRGB encoded. If false gammaIn must be set.
    bool                    isGammaOutSrgb;         // Dest texture should be sRGB encoded. If false gammaOut must be set.
    float                   gammaIn;                // Gamma level for the source texture if isGammaInSrgb == false.
    float                   gammaOut;               // Gamma level the dest texture if isGammaOutSrgb == false.
    ATC_ColorGamut          colorGamutIn;           // Color space for the source texture.
    ATC_ColorGamut          colorGamutOut;          // Color space for the dest texture. This is only supported on MacOS and should be set to atcColorGamutUnknown on other platforms.
                                                    // On MacOS color gamut will be converted from input to output as necessary.

    uint32_t                maxMipmaps;             // Number of mip-levels to compress/decompress.
    ATC_MipmapFilter        mipmapFilter;           // Mip-filter to use when generating mipmaps before compression.

    uint32_t                maxExtent;              // Maximum texture extent. Textures will be downsized to fit before compression.
    ATC_RoundMode           resizeRoundMode;        // Resize rounding mode to use if resizing before compression.
    ATC_ResizeFilter        resizeFilter;           // Resizing filter to use if resizing before compression.

    ATC_WrapMode            wrapMode;               // Wrap mode to use when generating mipmaps or resizing the source texture.

    bool                    useRgbmEncoding;        // Compress to/decompress from RGBm encoding.
    float                   rgbmRange;              // Range to use for RGBm encoding.

    bool                    cropUniformContent;     // Crop the source texture to remove uniform content before compressing.

    bool                    disableMultithreading;  // Disable multi-threading of compression operations. Has no effect on single-threaded compressors.

    bool                    flipX;                  // Flip the source texture along the X axis before compressing.
    bool                    flipY;                  // Flip the source texture along the Y axis before compressing.
    bool                    flipZ;                  // Flip the source texture along the Z axis before compressing.

    bool                    isNormalMap;            // Use normal map encoding.

    bool                    scaleRange;             // Scale the source texture to the entire color range before compressing.

    bool                    useSrgbFormat;          // Use sRGB pixel format for the dest texture.
    bool                    isASTCHDR;              // The source format of an ASTC KTX texture should be treated as ASTC HDR.

    ATC_FileFormat          fileFormat;             // File format to use for the destination texture.
    bool                    disableAnnotation;      // Disable annotation when writing KTX files.

    // Compression options used only when compressing with the atcCompressorPVRTC compressor.
    ATC_ChannelWeighting    channelWeighting;       // Channel weightings to use when compressing.
    ATC_PVRPunchThrough     pvrtcPunchThroughMode;  // PVRTC punch-through mode to use when compressing.

	uint32_t				comparisonChannels;		// Number of channels to consider when comparing textures. Defaults to 3.
} ATC_Options;

// Error metrics returned by ATC_Compare functions.
typedef struct ATC_ErrorMetrics
{
    double  MSE;                    // Mean square error.
    double  RMS;                    // Root mean square error.
    double  PSNR;                   // Peak signal-to-noise ratio.
    bool    bTexturesAreIdentical;  // The textures compared are identical.
} ATC_ErrorMetrics;

// Severity level of AppleTextureConverter feedback info.
typedef enum
{
    atcInfoMessage,         // Feedback on operation progress.
    atcInfoWarning,         // Operation has succeeded, but results may be incorrect.
    atcInfoError,           // Error while performing requested operation. Operation has failed.
} ATC_InfoLevel;

// Callback function for allocating memory. Returns a pointer to the allocated memory.
typedef void* (*ATC_MemoryAllocator)(
    void* userData,         // User-defined value from the ATC_Context.
    size_t size );          // Size of memory to allocate.

// Callback function for deallocating memory
typedef void (*ATC_MemoryDeallocator)(
    void* userData,         // User-defined value from the ATC_Context.
    void* data );           // Pointer to the memory to de-allocate.

// Callback function for feedback from AppleTextureConverter operations.
typedef void (*ATC_MessageCallback)(
    void* userData,         // User-defined value from the ATC_Context.
    ATC_InfoLevel level,    // Severity level of the feedback info.
    const char* string );   // String text of the feedback info.

// Struct declaring the compression/decompression context
typedef struct ATC_Context
{
    ATC_MemoryAllocator     memoryAllocator;    // (Optional) Memory allocator function to use. If set memoryDeallocator must also be set.
    ATC_MemoryDeallocator   memoryDeallocator;  // (Optional) Memory deallocator function to be use.
    ATC_MessageCallback     infoMessageHandler; // (Optional) Call back for info/warning/error text. If not set will default to stdout/stderr.
    void*                   userData;           // (Optional) User-data value passed to callback functions.
} ATC_Context;

// Struct describing compressor capabilities & options.
typedef struct ATC_CompressorInfo
{
    uint32_t        numFormats;         // Number of compressed formats supported by the compressor.
    ATC_Format*     formats;            // Array of compressed formats supported by the compressor.
    uint32_t        numQualityLevels;   // Number of quality levels supported by the compressor.
    ATC_Quality*    qualityLevels;      // Array of quality levels supported by the compressor.

    struct {
        bool isThreadSafe : 1;          // True if the compressor has been verifed to be thread-safe
        bool isMultithreaded : 1;       // True if the compressor is itself multi-threaded
    } Flags;
} ATC_CompressorInfo;

// Initialise ATC_Options to default options.
// Use of ATC_InitialiseDefaultOptions is optional and may be skipped if you set all the parameters explicitly.
bool ATC_InitialiseDefaultOptions(
    ATC_Options* options);         // Pointer to the ATC_Options struct to initialise.

// Query the capabilties & options supported for a given compressor.
const ATC_CompressorInfo* ATC_QueryCompressorInfo(
    const ATC_Context* context,     // (Optional) Pointer to an ATC_Context object. If null a default context is used.
    ATC_Compressor compressor);     // Compressor ID to query.

// Create a 2D Texture
ATC_Error ATC_CreateTexture2D(
    const ATC_Context*  context,        // (Optional) Pointer to an ATC_Context object. If null a default context is used.
    uint32_t            width,          // Width of the texture. Must be greater than zero & not greater than atcMaxExtent.
    uint32_t            height,         // Height of the texture. Must be greater than zero & not greater than atcMaxExtent.
    uint32_t            numMipLevels,   // Number of mip-levels. Must not exceed atcMaxMipMaps.
    ATC_Format          format,         // Format of the texture.
    ATC_ColorGamut      colorGamut,     // Color gamut of the texture, sRGB or DisplayP3 if it is a color. Must be atcColorGamutNone for normal maps.
    bool                isNormalMap,    // Texture is a normal map.
    const ATC_Texture** texture);       // (Out) Pointer to the created texture.

// Create a 2D Texture Array
ATC_Error ATC_CreateTexture2DArray(
    const ATC_Context*  context,        // (Optional) Pointer to an ATC_Context object. If null a default context is used.
    uint32_t            width,          // Width of the texture. Must be greater than zero & not greater than atcMaxExtent.
    uint32_t            height,         // Height of the texture. Must be greater than zero & not greater than atcMaxExtent.
    uint32_t            elements,       // Number of elements. Must be greater than one & not greater than atcMaxElements.
    uint32_t            numMipLevels,   // Number of mip-levels. Must not exceed atcMaxMipMaps.
    ATC_Format          format,         // Format of the texture.
    ATC_ColorGamut      colorGamut,     // Color gamut of the texture, sRGB or DisplayP3 if it is a color. Must be atcColorGamutNone for normal maps.
    bool                isNormalMap,    // Texture is a normal map.
    const ATC_Texture** texture);       // (Out) Pointer to the created texture.

// Create a 3D Texture
ATC_Error ATC_CreateTexture3D(
    const ATC_Context*  context,        // (Optional) Pointer to an ATC_Context object. If null a default context is used.
    uint32_t            width,          // Width of the texture. Must be greater than zero & not greater than atcMaxExtent.
    uint32_t            height,         // Height of the texture. Must be greater than zero & not greater than atcMaxExtent.
    uint32_t            depth,          // Depth of the texture. Must be greater than one & not greater than atcMaxDepth.
    uint32_t            numMipLevels,   // Number of mip-levels. Must not exceed atcMaxMipMaps.
    ATC_Format          format,         // Format of the texture.
    ATC_ColorGamut      colorGamut,     // Color gamut of the texture, sRGB or DisplayP3 if it is a color. Must be atcColorGamutNone for normal maps.
    bool                isNormalMap,    // Texture is a normal map.
    const ATC_Texture** texture);       // (Out) Pointer to the created texture.

// Create a Cubemap Texture
ATC_Error ATC_CreateTextureCube(
    const ATC_Context*  context,        // (Optional) Pointer to an ATC_Context object. If null a default context is used.
    uint32_t            size,           // Width & Height of the cube faces. Must be greater than zero & not greater than atcMaxExtent.
    uint32_t            numMipLevels,   // Number of mip-levels. Must not exceed atcMaxMipMaps.
    ATC_Format          format,         // Format of the texture.
    ATC_ColorGamut      colorGamut,     // Color gamut of the texture, sRGB or DisplayP3 if it is a color. Must be atcColorGamutNone for normal maps.
    bool                isNormalMap,    // Texture is a normal map.
    const ATC_Texture** texture);       // (Out) Address of the created texture pointer.

// Free the memory used for a texture through the ATC_MemoryDeallocator function defined in ATC_Context.
ATC_Error ATC_DeleteTexture(
    const ATC_Context*  context,        // (Optional) Pointer to an ATC_Context object. If null a default context is used.
    const ATC_Texture*  texture,        // Pointer to the ATC_Texture to delete.
    bool                freeSurfaces);  // Bool indicating whether to also free the memory allocatted for surface data.

// Retrieve a pointer to the specified surface.
ATC_Error ATC_GetSurface(
    const ATC_Context*  context,        // (Optional) Pointer to an ATC_Context object. If null a default context is used.
    const ATC_Texture*  texture,        // Pointer to the ATC_Texture .
    uint32_t            elementOrFace,  // The element, face or slice of the texture to access, depending on the texture type.
    uint32_t            mipLevel,       // The mip-level of the texture to access.
    ATC_Surface**       pSurface);      // (Out) Address of the surface pointer.

// Compress texture from memory to memory. Destination memory will be allocated using the ATC_MemoryAllocator function defined in ATC_Context.
ATC_Error ATC_CompressMemory(
	const ATC_Context* context,		// (Optional) Pointer to an ATC_Context object. If null a default context is used.
	const ATC_Texture* sourceTex,	// Pointer to the source texture. The source texture must be an uncompressed format.
    const ATC_Texture** destTex,	// (Out) Pointer to the destination texture created by this function.
	const ATC_Options* options);	// Pointer to the options defining how to compress the texture.

// Compress texture from file to file.
ATC_Error ATC_CompressFile(
	const ATC_Context* context,		// (Optional) Pointer to an ATC_Context object. If null a default context is used.
	const char* sourceFile,			// File path for the source texture. The source texture must be an uncompressed format.
	const char* destFile,			// File path for the destination texture. The destination texture will be overwritten.
	const ATC_Options* options);	// Pointer to the options defining how to compress the texture.

// Decompress texture from memory to memory. Destination memory will be allocated using the ATC_MemoryAllocator function defined in ATC_Context.
ATC_Error ATC_DecompressMemory(
	const ATC_Context* context,		// (Optional) Pointer to an ATC_Context object. If null a default context is used.
	const ATC_Texture* sourceTex, 	// Pointer to the source texture. The source texture must be a compressed format.
    const ATC_Texture** destTex,	// (Out) Pointer to the destination texture created by this function.
	const ATC_Options* options);	// Pointer to the options defining how to decompress the texture.

// Decompress texture from file to file.
ATC_Error ATC_DecompressFile(
	const ATC_Context* context,		// (Optional) Pointer to an ATC_Context object. If null a default context is used.
	const char* sourceFile,			// File path for the source texture. The source texture must be a compressed format.
	const char* destFile,			// File path for the destination texture. The destination texture will be overwritten.
	const ATC_Options* options);	// Pointer to the options defining how to decompress the texture.

// Compare two in-memory textures.
ATC_Error ATC_CompareMemory(
	const ATC_Context* context,		// (Optional) Pointer to an ATC_Context object. If null a default context is used.
	const ATC_Texture* refImage,	// Pointer to the reference texture.
	const ATC_Texture* compImage,   // Pointer to the comparison texture.
	const ATC_Options* options,     // Pointer to the options defining how to compare the textures.
	ATC_ErrorMetrics* metrics);     // Pointer to the ATC_ErrorMetrics. ATC_CompareMemory will populate this struct.

// Compare two texture files.
ATC_Error ATC_CompareFile(
	const ATC_Context* context,		// (Optional) Pointer to an ATC_Context object. If null a default context is used.
	const char* refFile,            // File path for the reference texture.
	const char* compFile,           // File path for the comparison texture.
	const ATC_Options* options,     // Pointer to the options defining how to compare the textures.
	ATC_ErrorMetrics* metrics);     // Pointer to the ATC_ErrorMetrics. ATC_CompareFile will populate this struct.

// When writing to KTX files additional meta-data may be written.

extern const char* ATC_KeyMtlPixelFormat;
// "KTXmetalPixelFormat", as defined in the KTX spec (https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html), section 5.3.3
// This describes the MTLPixelFormat for the texture data. It is only set for textures where the format can not be determined from
// the file header. This currently means ASTC LDR & HDR textures when saved to KTX 1.1 files.

extern const char* ATC_KeyPremultipliedAlpha;
// "com.apple.image.premultipliedAlpha". This indicates that the texture data has pre-multiplied alpha. The value is written as a
// 32-bit unsigned integer with zero indicating false & non-zero indicating true.

extern const char* ATC_KeyColorGamut;
// "com.apple.image.colorGamut". Specifies the color gamut of the image. The value is written as a 0 terminated string.
// It will be written if the color space can be read from the source image or --gamma_in is used to specify it.
// Values are:
//  - 'sRGB'
//  - 'DisplayP3'

extern const char* ATC_KeyColorTransfer;
// "com.apple.image.colorTransfer". Specifies the color transfer function of the image. The value is written as a 0 terminated string.
// Values are:
//  - 'sRGB'
//  - 'linear'

#ifdef __cplusplus
}
#endif


/*

Using AppleTextureConverter
---------------------------
AppleTextureConverter provides a simple API for compressing, decompressing & comparing textures.
Textures can be either in memory or from file.
For example usage check below.

Building with AppleTextureConverter on macOS
--------------------------------------------
In Xcode's Project Editor/Build Settings/Search Paths, Add ${SYSTEM_DEVELOPER_USR_DIR}/include to Header Search Paths & add ${SYSTEM_DEVELOPER_USR_DIR}/lib to Library Search Paths.
In Xcode's Project Editor/Build Settings/Linking - General, Add -lAppleTextureConverter to Other Linker Flags.

You will also need to link with several system frameworks: (CoreFoundation/Foundation, AppKit, Metal, ImageIO & Accelerate).

Building with AppleTextureConverter on Windows
----------------------------------------------
In Visual Studio edit the properties for your project. C/C++ - General - Additional Include Directories - "C:\Program Files\Metal Developer Tools\include"
In Visual Studio edit the properties for your project. Linker - General - Additional Library Directories - "C:\Program Files\Metal Developer Tools\lib"


Example Usage
-------------

static void* MemoryAllocator( void* userData, size_t size )
{
    return malloc(size);
}

static void MemoryDeallocator( void* userData, void* data )
{
    free(data);
}

static void MessageCallback( void* userData, ATC_InfoLevel level, const char* string )
{
    printf("ATC_Test: %s", string);
}

ATC_Context globalContext = { MemoryAllocator, MemoryDeallocator, MessageCallback, 0 };

bool ExampleFileCompression( const char* pszSourceFile, const char* pszDestFile )
{
    ATC_Options options;
    ATC_InitialiseDefaultOptions( &options );
    options.format = atcFormatBc3Unorm;
    options.quality = atcQualityHighest;

    return ( ATC_CompressFile( &globalContext, pszSourceFile, pszDestFile, &options ) == atcErrorNone );
}

bool ExampleMemoryCompression()
{
    ATC_Options options;
    ATC_InitialiseDefaultOptions(&options);
    options.format = atcFormatBc3Unorm;
    options.quality = atcQualityHighest;

    const ATC_Texture* src = NULL;
    ATC_CreateTexture2D( &globalContext, 4, 4, 1, atcFormatRgba8Unorm, atcColorGamutUnknown, false, &src );

    ATC_Surface* surface = NULL;
    ATC_GetSurface( &globalContext, src, 0, 0, &surface );
    surface->width = 4;
    surface->height = 4;
    surface->data = test_block_rgba;
    surface->size = test_block_rgba_size;

    const ATC_Texture* dest = NULL;
    bool success = ( ATC_CompressMemory( &globalContext, src, &dest, &options ) == atcErrorNone );
    if ( success )
    {
        // Use compressed image
        for (uint32_t mipLevel = 0; mipLevel < dest->numMipLevels; ++mipLevel)
        {
            ATC_Surface* surface = NULL;
            ATC_GetSurface( &globalContext, dest, 0, mipLevel, &surface);
            void* data = surface->data;
        }
    }

    ATC_DeleteTexture( &globalContext, src, false );
    ATC_DeleteTexture( &globalContext, dest, true );

    return success;
}

bool ExampleFileDecompression( const char* pszSourceFile, const char* pszDestFile )
{
    ATC_Options options;
    ATC_InitialiseDefaultOptions(&options);
    options.format = atcFormatRgba8Unorm;

    return ( ATC_DecompressFile( &globalContext, pszSourceFile, pszDestFile, &options ) == atcErrorNone );
}

bool ExampleMemoryDecompression()
{
    ATC_Options options;
    ATC_InitialiseDefaultOptions(&options);
    options.format = atcFormatRgba8Unorm;

    const ATC_Texture* src = NULL;
    ATC_CreateTexture2D( &globalContext, 4, 4, 1, atcFormatBc3Unorm, atcColorGamutUnknown, false, &src );

    ATC_Surface* surface = NULL;
    ATC_GetSurface( &globalContext, src, 0, 0, &surface );
    surface->width = 4;
    surface->height = 4;
    surface->data = test_block_bc3;
    surface->size = test_block_bc3_size;

    const ATC_Texture* dest = NULL;
    bool success = ( ATC_DecompressMemory( &globalContext, src, &dest, &options ) == atcErrorNone );
    if ( success )
    {
        // Use decompressed texture
        for (uint32_t mipLevel = 0; mipLevel < dest->numMipLevels; ++mipLevel)
        {
            ATC_Surface* surface = NULL;
            ATC_GetSurface( &globalContext, dest, 0, mipLevel, &surface);
            void* data = surface->data;
        }
    }

    // Memory may have been allocated regardless of success
    ATC_DeleteTexture( &globalContext, src, false );
    ATC_DeleteTexture( &globalContext, dest, true );

    return success;
}

*/
