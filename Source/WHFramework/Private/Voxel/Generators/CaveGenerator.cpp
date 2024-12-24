// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/CaveGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"

void CaveGenerator::GenerateCave(AVoxelChunk* chunk, UVoxelModule* info)
{
	//晶格大小
	int32 m = 2;

	for (int i = 0; i < UVoxelModule::Get().GetWorldData().ChunkSize.X; ++i)
		for (int j = 0; j < UVoxelModule::Get().GetWorldData().ChunkSize.Y; ++j)
		{
			int32 mh = chunk->GetTopography(FIndex(i, j)).Height;
			for (int k = mh; k >= FMath::Max(mh - 10, 0); --k)
			{
				FVector pf = FVector(
					float(i) / UVoxelModule::Get().GetWorldData().ChunkSize.X / m,
					float(j) / UVoxelModule::Get().GetWorldData().ChunkSize.Y / m,
					float(k) / UVoxelModule::Get().GetWorldData().ChunkSize.Z / m
				);

				//若高于一定阈值，挖空
				if ((UVoxelModule::Get().GetNoise3D(pf + FVector(chunk->GetIndex().X, chunk->GetIndex().Y, 0) / 2, FVector(1.f, 1.f, 1.f)) + (mh - k) / 10.0f * 0.3f) > 0.5f)
				{
					info->SetVoxelByIndex(chunk->LocalIndexToWorld(FIndex(i, j, k)), FVoxelItem::Empty);
				}

				mh = FMath::Max(mh, k);
			}

			chunk->GetTopography(FIndex(i, j)).Height = mh;
		}
}
