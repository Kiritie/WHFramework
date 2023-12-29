// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widget/Common/CommonButton.h"
#include "WidgetSettingPageItemBase.generated.h"

class UWidgetSettingPageBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetSettingPageItemBase : public UCommonButton
{
	GENERATED_BODY()

public:
	UWidgetSettingPageItemBase(const FObjectInitializer& ObjectInitializer);

public:
	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

protected:
	virtual void NativeOnSelected(bool bBroadcast) override;

	virtual void NativeOnDeselected(bool bBroadcast) override;

protected:
	UPROPERTY(BlueprintReadOnly)
	UWidgetSettingPageBase* SettingPage;
};
