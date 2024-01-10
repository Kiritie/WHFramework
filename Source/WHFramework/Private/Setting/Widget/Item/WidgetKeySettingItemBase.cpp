// Fill out your copyright notice in the Description Item of Project Settings.

#include "Setting/Widget/Item/WidgetKeySettingItemBase.h"

#include "Setting/Widget/Misc/WidgetPressAnyKeyPanelBase.h"
#include "Widget/WidgetModuleStatics.h"
#include "Widget/Common/CommonButton.h"

UWidgetKeySettingItemBase::UWidgetKeySettingItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UWidgetKeySettingItemBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void UWidgetKeySettingItemBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	Keys.Empty();
}

void UWidgetKeySettingItemBase::OnCreate(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);

	for(int32 i = 0; i < Btn_Values.Num(); i++)
	{
		Btn_Values[i]->OnClicked().AddUObject(this, &UWidgetKeySettingItemBase::OnValueButtonClicked, i);
	}
}

void UWidgetKeySettingItemBase::OnInitialize(const TArray<FParameter>& InParams)
{
	Super::OnInitialize(InParams);
}

void UWidgetKeySettingItemBase::OnRefresh()
{
	Super::OnRefresh();

	for(int32 i = 0; i < Keys.Num(); i++)
	{
		Btn_Values[i]->SetIconBrush(FSlateBrush());
		Btn_Values[i]->SetTitle(FText::GetEmpty());
		const UCommonInputPlatformSettings* Settings = UPlatformSettingsManager::Get().GetSettingsForPlatform<UCommonInputPlatformSettings>();
		FSlateBrush ImageBrush;
		if(Settings->TryGetInputBrush(ImageBrush, Keys[i], ECommonInputType::MouseAndKeyboard, FName("XSX")))
		{
			ImageBrush.ImageSize = FVector2D(28.f);
			Btn_Values[i]->SetIconBrush(ImageBrush);
		}
		else
		{
			Btn_Values[i]->SetTitle(Keys[i].GetDisplayName(false));
		}
	}
}

void UWidgetKeySettingItemBase::OnDestroy(bool bRecovery)
{
	Super::OnDestroy(bRecovery);
}

void UWidgetKeySettingItemBase::OnValueButtonClicked(int32 InIndex)
{
	UWidgetPressAnyKeyPanelBase* PressAnyKeyPanel = UWidgetModuleStatics::CreateUserWidget<UWidgetPressAnyKeyPanelBase>();
	PressAnyKeyPanel->OnKeySelected.AddUObject(this, &ThisClass::OnKeySelected, PressAnyKeyPanel);
	PressAnyKeyPanel->OnKeySelectionCanceled.AddUObject(this, &ThisClass::OnKeySelectionCanceled, PressAnyKeyPanel);
	PressAnyKeyPanel->Open({ InIndex });
}

void UWidgetKeySettingItemBase::OnKeySelected(FKey InKey, UWidgetPressAnyKeyPanelBase* InPressAnyKeyPanel)
{
	InPressAnyKeyPanel->OnKeySelected.RemoveAll(this);
	Keys[InPressAnyKeyPanel->GetWidgetParams()[0].GetIntegerValue()] = InKey;
	if(OnValuesChanged.IsBound())
	{
		OnValuesChanged.Broadcast(this, GetValues());
	}
	Refresh();
}

void UWidgetKeySettingItemBase::OnKeySelectionCanceled(UWidgetPressAnyKeyPanelBase* InPressAnyKeyPanel)
{
	InPressAnyKeyPanel->OnKeySelectionCanceled.RemoveAll(this);
}

TArray<FParameter> UWidgetKeySettingItemBase::GetValues() const
{
	TArray<FParameter> Values;
	for(int32 i = 0; i < Keys.Num(); i++)
	{
		Values.Add(Keys[i]);
	}
	return Values;
}

void UWidgetKeySettingItemBase::SetValues(const TArray<FParameter>& InValues)
{
	Keys.Empty();
	for(int32 i = 0; i < InValues.Num(); i++)
	{
		Keys.Add(InValues[i].GetKeyValue());
	}
	Super::SetValues(InValues);
}
