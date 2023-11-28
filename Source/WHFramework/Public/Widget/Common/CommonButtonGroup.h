// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Groups/CommonButtonGroupBase.h"
#include "ObjectPool/ObjectPoolInterface.h"

#include "CommonButtonGroup.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UCommonButtonGroup : public UCommonButtonGroupBase, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UCommonButtonGroup();
	
	//////////////////////////////////////////////////////////////////////////
	/// Object
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;
};
