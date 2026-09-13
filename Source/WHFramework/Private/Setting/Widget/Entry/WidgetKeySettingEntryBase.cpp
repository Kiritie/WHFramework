#include "Setting/Widget/Entry/WidgetKeySettingEntryBase.h"

#include "CommonInputBaseTypes.h"
#include "Setting/SettingModuleTypes.h"
#include "Setting/Widget/Misc/WidgetPressAnyKeyPanelBase.h"
#include "Widget/Common/CommonButton.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetKeySettingEntryBase::UWidgetKeySettingEntryBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWidgetKeySettingEntryBase::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);
}

void UWidgetKeySettingEntryBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	if(Btn_Value)
	{
		Btn_Value->OnClicked().RemoveAll(this);
	}
	Super::OnDespawn_Implementation(InMode);
}

void UWidgetKeySettingEntryBase::ApplyValueToControl_Implementation(const FParameter& InValue)
{
	if(!Btn_Value)
	{
		return;
	}
	Btn_Value->SetStyleTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Style.Button.Setting.Key")), false));
	Btn_Value->OnClicked().RemoveAll(this);
	Btn_Value->OnClicked().AddUObject(this, &ThisClass::OnValueClicked);
	const FKey* Key = InValue.GetPtr<FKey>();
	Btn_Value->SetIconBrush(FSlateBrush());
	Btn_Value->SetTitle(Key ? Key->GetDisplayName(false) : FText::GetEmpty());
	if(!Key)
	{
		return;
	}

	const UCommonInputPlatformSettings* Settings = UCommonInputPlatformSettings::Get();
	FSlateBrush ImageBrush;
	if(Settings && Settings->TryGetInputBrush(ImageBrush, *Key, ECommonInputType::MouseAndKeyboard, FName(TEXT("XSX"))))
	{
		ImageBrush.ImageSize = FVector2D(28.f);
		Btn_Value->SetIconBrush(ImageBrush);
		Btn_Value->SetTitle(FText::GetEmpty());
	}
}

void UWidgetKeySettingEntryBase::OnValueClicked()
{
	if(UWidgetPressAnyKeyPanelBase* Panel = UWidgetModuleStatics::CreateUserWidget<UWidgetPressAnyKeyPanelBase>())
	{
		Panel->OnKeySelected.AddUObject(this, &ThisClass::OnKeySelected, Panel);
		Panel->OnKeySelectionCanceled.AddUObject(this, &ThisClass::OnKeySelectionCanceled, Panel);
		Panel->Open(FWidgetPressAnyKeyOpenParameter());
	}
}

void UWidgetKeySettingEntryBase::OnKeySelected(FKey InKey, UWidgetPressAnyKeyPanelBase* InPanel)
{
	if(InPanel)
	{
		InPanel->OnKeySelected.RemoveAll(this);
		InPanel->OnKeySelectionCanceled.RemoveAll(this);
	}
	CommitUserValue(FParameter(InKey));
}

void UWidgetKeySettingEntryBase::OnKeySelectionCanceled(UWidgetPressAnyKeyPanelBase* InPanel)
{
	if(InPanel)
	{
		InPanel->OnKeySelected.RemoveAll(this);
		InPanel->OnKeySelectionCanceled.RemoveAll(this);
	}
}
