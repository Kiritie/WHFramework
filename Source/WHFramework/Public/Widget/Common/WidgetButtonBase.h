// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonButtonBase.h"

#include "WidgetButtonBase.generated.h"

class UCommonTextBlock;

UCLASS(Abstract, BlueprintType, Blueprintable)
class WHFRAMEWORK_API UWidgetButtonBase : public UCommonButtonBase
{
	GENERATED_BODY()

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
