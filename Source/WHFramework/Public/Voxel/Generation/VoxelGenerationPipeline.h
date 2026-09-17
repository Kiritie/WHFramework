#pragma once
#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSectionStorage.h"
#include "Voxel/Generation/VoxelGenerationContext.h"
#include "Voxel/Generation/Kernel/VoxelGenQuery.h"
class WHFRAMEWORK_API FVoxelGenerationPipeline
{
public:
    explicit FVoxelGenerationPipeline(const FVoxelGenerationRuntimeConfig& In):Config(In){}
    bool GenerateSection(const FVoxelSectionKey& Key,FVoxelSectionStorage& Out,const std::atomic_bool* Cancel=nullptr) const;
    bool SampleBaseBlock(const FIntVector& Position,FVoxelBlockState& Out) const;
    FVoxelColumnSample SampleColumn(int32 X,int32 Y) const;
    bool BuildHandshakeSignature(uint64& Out) const;
    bool BuildQuery(const VoxelGen::Box& Bounds,bool bVisualOnly,std::unique_ptr<VoxelGen::Query>& Out,FString& Error,const std::atomic_bool* Cancel=nullptr) const;
    const FVoxelGenerationRuntimeConfig& GetConfig() const{return Config;}
    bool ToRuntime(VoxelGen::Cell Source,FVoxelBlockState& Out) const;
    bool ToSymbol(FVoxelBlockState Source,VoxelGen::Cell& Out) const;
private:
    FVoxelGenerationRuntimeConfig Config;
};
