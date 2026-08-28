// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHActor.h"
#include "VoxelRoot.generated.h"

/**
 * 体素模块
 */
UCLASS()
class WHFRAMEWORK_API AVoxelRoot : public AWHActor
{
	GENERATED_BODY()

public:
	AVoxelRoot();

public:
#if WITH_EDITOR
	virtual bool IsUserManaged() const override { return false; }

	virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
#endif
};
