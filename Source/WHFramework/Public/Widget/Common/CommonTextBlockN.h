// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectPool/ObjectPoolInterface.h"

#include "CommonTextBlock.h"

#include "CommonTextBlockN.generated.h"

UCLASS(BlueprintType, Blueprintable, Config = CommonUI, DefaultConfig, ClassGroup = UI, meta = (Category = "Common UI", DisplayName = "Common Text N", PrioritizeCategories = "Content"))
class WHFRAMEWORK_API UCommonTextBlockN : public UCommonTextBlock, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UCommonTextBlockN(const FObjectInitializer& ObjectInitializer);
	
	//////////////////////////////////////////////////////////////////////////
	/// Object
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;
};
