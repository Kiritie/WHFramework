// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/GameInstance.h"
#include "WHGameInstance.generated.h"

/**
 * 游戏实例基类
 */
UCLASS()
class WHFRAMEWORK_API UWHGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UWHGameInstance();

public:
	virtual void Init() override;

	virtual void Shutdown() override;
};
