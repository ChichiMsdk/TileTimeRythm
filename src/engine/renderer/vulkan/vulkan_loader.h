#ifndef VULKAN_LOADER_H
#define VULKAN_LOADER_H

#include "yvulkan.h"

typedef struct GeoSurface
{
	uint32_t	startIndex;
	uint32_t	count;
} GeoSurface;

typedef struct MeshAsset
{
    char*			pName;
	GeoSurface*		pSurfaces;
    GpuMeshBuffers	meshBuffers;
} MeshAsset;


#endif // VULKAN_LOADER_H
