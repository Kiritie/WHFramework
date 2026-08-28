// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Root/VoxelRoot.h"

// Sets default values
AVoxelRoot::AVoxelRoot()
{
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}
