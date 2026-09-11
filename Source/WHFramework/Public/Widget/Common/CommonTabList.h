// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonTabListWidgetBase.h"
#include "ObjectPool/ObjectPoolInterface.h"

#include "CommonTabList.generated.h"

UCLASS(BlueprintType, Blueprintable, Config = CommonUI, DefaultConfig, ClassGroup = UI, meta = (Category = "Common UI", DisplayName = "Common Tab List", PrioritizeCategories = "Content"))
class WHFRAMEWORK_API UCommonTabList : public UCommonTabListWidgetBase, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UCommonTabList(const FObjectInitializer& ObjectInitializer);
	
	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:

	virtual void OnSpawn_Implementation(
		const FParameter& InParameter) override;
	
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode)
		override;
};
