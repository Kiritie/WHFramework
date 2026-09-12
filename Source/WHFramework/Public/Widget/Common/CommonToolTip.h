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

	virtual void OnSpawn_Implementation(const FParameter& InParam) override;
		
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode)
		override;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UCommonTextBlockN* Txt_Content;

public:
	UFUNCTION(BlueprintPure)
	FText GetContent() const;

	UFUNCTION(BlueprintCallable)
	void SetContent(const FText InContent);
};
