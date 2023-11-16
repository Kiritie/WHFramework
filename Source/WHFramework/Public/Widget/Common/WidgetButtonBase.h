// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonButtonBase.h"
#include "ObjectPool/ObjectPoolInterface.h"

#include "WidgetButtonBase.generated.h"

class UCommonTextBlock;

UCLASS(Abstract, BlueprintType, Blueprintable)
class WHFRAMEWORK_API UWidgetButtonBase : public UCommonButtonBase, public IObjectPoolInterface
{
	GENERATED_BODY()
	
	//////////////////////////////////////////////////////////////////////////
	/// Object
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void NativeOnCurrentTextStyleChanged() override;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UCommonTextBlock* Txt_Title;

protected:
	UPROPERTY(EditAnywhere, Category = "Content")
	FText ButtonText;

public:
	UFUNCTION(BlueprintPure)
	FText GetButtonText() const { return ButtonText; }

	UFUNCTION(BlueprintCallable)
	void SetButtonText(const FText InButtonText);
};
