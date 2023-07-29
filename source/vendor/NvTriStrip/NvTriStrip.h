#ifndef NVTRISTRIP_H
#define NVTRISTRIP_H

#ifndef NULL
#define NULL 0
#endif

//#pragma comment(lib, "nvtristrip")

////////////////////////////////////////////////////////////////////////////////////////
// Public interface for stripifier
////////////////////////////////////////////////////////////////////////////////////////

//GeForce1 and 2 cache size
#define CACHESIZE_GEFORCE1_2 16

//GeForce3 cache size
#define CACHESIZE_GEFORCE3   24

#ifdef __cplusplus
extern "C" {
#else
#ifndef bool
typedef bool int;
#endif
#endif

typedef enum {
	NV_PT_LIST,
	NV_PT_STRIP,
	NV_PT_FAN
} NvPrimType;

typedef struct {
	NvPrimType type;
	unsigned int numIndices;
	unsigned short * indices;
} NvPrimitiveGroup;

inline NvPrimitiveGroup NvPrimitiveGroupDef()
{
	NvPrimitiveGroup g  =  {
		NV_PT_STRIP,
		0,
		NULL };
	return g;
}

void NvDestroyPrimGroup(NvPrimitiveGroup * g);

void NvDestroyPrimGroups(NvPrimitiveGroup ** g, unsigned int count);

////////////////////////////////////////////////////////////////////////////////////////
// NvEnableRestart()
//
// For GPUs that support primitive restart, this sets a value as the restart index
//
// Restart is meaningless if strips are not being stitched together, so enabling restart
//  makes NvTriStrip forcing stitching.  So, you'll get back one strip.
//
// Default value: disabled
//
void NvEnableRestart(const unsigned int restartVal);

////////////////////////////////////////////////////////////////////////////////////////
// NvDisableRestart()
//
// For GPUs that support primitive restart, this disables using primitive restart
//
void NvDisableRestart();


////////////////////////////////////////////////////////////////////////////////////////
// NvSetCacheSize()
//
// Sets the cache size which the stripfier uses to optimize the data.
// Controls the length of the generated individual strips.
// This is the "actual" cache size, so 24 for GeForce3 and 16 for GeForce1/2
// You may want to play around with this number to tweak performance.
//
// Default value: 16
//
void NvSetCacheSize(const unsigned int cacheSize);


////////////////////////////////////////////////////////////////////////////////////////
// NvSetStitchStrips()
//
// bool to indicate whether to stitch together strips into one huge strip or not.
// If set to true, you'll get back one huge strip stitched together using degenerate
//  triangles.
// If set to false, you'll get back a large number of separate strips.
//
// Default value: true
//
void NvSetStitchStrips(const bool bStitchStrips);


////////////////////////////////////////////////////////////////////////////////////////
// NvSetMinStripSize()
//
// Sets the minimum acceptable size for a strip, in triangles.
// All strips generated which are shorter than this will be thrown into one big, separate list.
//
// Default value: 0
//
void NvSetMinStripSize(const unsigned int minSize);


////////////////////////////////////////////////////////////////////////////////////////
// NvSetListsOnly()
//
// If set to true, will return an optimized list, with no strips at all.
//
// Default value: false
//
void NvSetListsOnly(const bool bListsOnly);


////////////////////////////////////////////////////////////////////////////////////////
// NvGenerateStrips()
//
// in_indices: input index list, the indices you would use to render
// in_numIndices: number of entries in in_indices
// primGroups: array of optimized/stripified NvPrimitiveGroups
// numGroups: number of groups returned
//
// Be sure to call delete[] on the returned primGroups to avoid leaking mem
//
bool NvGenerateStrips(const unsigned short* in_indices, const unsigned int in_numIndices,
					NvPrimitiveGroup** primGroups, unsigned short* numGroups, bool validateEnabled);


////////////////////////////////////////////////////////////////////////////////////////
// NvRemapIndices()
//
// Function to remap your indices to improve spatial locality in your vertex buffer.
//
// in_primGroups: array of NvPrimitiveGroups you want remapped
// numGroups: number of entries in in_primGroups
// numVerts: number of vertices in your vertex buffer, also can be thought of as the range
//  of acceptable values for indices in your primitive groups.
// remappedGroups: array of remapped NvPrimitiveGroups
//
// Note that, according to the remapping handed back to you, you must reorder your 
//  vertex buffer.
//
// Credit goes to the MS Xbox crew for the idea for this interface.
//
void NvRemapIndices(const NvPrimitiveGroup* in_primGroups, const unsigned short numGroups, 
				  const unsigned short numVerts, NvPrimitiveGroup** remappedGroups);

#ifdef __cplusplus
}
#endif

#endif