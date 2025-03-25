// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/Base/WHObject.h"
#include "Thread/IThreadSafeInterface.h"
#include "VoxelGenerator.generated.h"

class UVoxelModule;
class AVoxelChunk;

/**
 *
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew)
class WHFRAMEWORK_API UVoxelGenerator : public UWHObject, public IThreadSafeInterface
{
	GENERATED_BODY()
	
public:
	UVoxelGenerator();
	
public:
	virtual void Initialize(UVoxelModule* InModule);

	virtual void Generate(AVoxelChunk* InChunk);

protected:
	UPROPERTY(BlueprintReadOnly)
	UVoxelModule* Module;
};
