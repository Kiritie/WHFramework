// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/Widget/Page/WidgetSettingPageItemBase.h"
#include "Setting/Widget/Page/WidgetSettingPageBase.h"

UWidgetSettingPageItemBase::UWidgetSettingPageItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bSelectable = true;
	bToggleable = false;

	SettingPage = nullptr;
}

void UWidgetSettingPageItemBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);

	SettingPage = Cast<UWidgetSettingPageBase>(InOwner);
}

void UWidgetSettingPageItemBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	SettingPage = nullptr;
}

void UWidgetSettingPageItemBase::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);

	if(SettingPage)
	{
		SettingPage->Open();
	}
}

void UWidgetSettingPageItemBase::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);
}
