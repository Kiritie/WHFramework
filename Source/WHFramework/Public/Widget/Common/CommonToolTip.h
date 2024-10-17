// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "ObjectPool/ObjectPoolInterface.h"

#include "CommonToolTip.generated.h"

class UImage;
class UCommonTextBlockN;

UCLASS(Abstract, BlueprintType, Blueprintable, ClassGroup = UI, meta = (Category = "Common UI", DisplayName = "Common ToolTip", DisableNativeTick))
class WHFRAMEWORK_API UCommonToolTip : public UUserWidget, public IObjectPoolInterface
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UCommonTextBlockN* Txt_Content;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UWidget* OwnerWidget;

public:
	UFUNCTION(BlueprintPure)
	FText GetContent() const;

	UFUNCTION(BlueprintCallable)
	void SetContent(const FText InContent);
};
