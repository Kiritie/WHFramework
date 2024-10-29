// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectPool/ObjectPoolInterface.h"
#include "Widget/Pool/PoolWidgetBase.h"

#include "CommonToolTip.generated.h"

class UImage;
class UCommonTextBlockN;

UCLASS(Abstract, BlueprintType, Blueprintable, ClassGroup = UI, meta = (Category = "Common UI", DisplayName = "Common ToolTip", DisableNativeTick))
class WHFRAMEWORK_API UCommonToolTip : public UPoolWidgetBase
{
	GENERATED_BODY()

public:
	UCommonToolTip(const FObjectInitializer& ObjectInitializer);
	
	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UCommonTextBlockN* Txt_Content;

public:
	UFUNCTION(BlueprintPure)
	FText GetContent() const;

	UFUNCTION(BlueprintCallable)
	void SetContent(const FText InContent);
};
