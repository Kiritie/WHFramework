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
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
	
	virtual void OnDespawn_Implementation(bool bRecovery) override;
};
