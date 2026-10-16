#include "Setting/Widget/Entry/WidgetKeySettingEntryBase.h"

#include "CommonInputBaseTypes.h"
#include "Setting/SettingModuleTypes.h"
#include "Setting/Widget/Misc/WidgetPressAnyKeyPanelBase.h"
#include "Widget/Common/CommonButton.h"
#include "Widget/WidgetModuleStatics.h"
#include "Setting/SettingEntry.h"

UWidgetKeySettingEntryBase::UWidgetKeySettingEntryBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UWidget* UWidgetKeySettingEntryBase::GetValueControl() const
{
	return Btn_Value;
}

FName UWidgetKeySettingEntryBase::GetRendererName() const
{
	return TEXT("Key");
}

void UWidgetKeySettingEntryBase::BindValueControl()
{
	if (Btn_Value)
	{
		Btn_Value->OnClicked().RemoveAll(this);
		Btn_Value->OnClicked().AddUObject(this, &ThisClass::OnValueClicked);
	}
}

void UWidgetKeySettingEntryBase::UnbindValueControl()
{
	ReleaseCapture(true);
	if(Btn_Value)
	{
		Btn_Value->OnClicked().RemoveAll(this);
	}
}

void UWidgetKeySettingEntryBase::OnModelStateChanged()
{
	if (!SettingEntry || !SettingEntry->IsEnabled() || !SettingEntry->IsVisible())
	{
		ReleaseCapture(true);
	}
}

void UWidgetKeySettingEntryBase::ApplyValueToControl_Implementation(const FParameter& InValue)
{
	if(!Btn_Value)
	{
		return;
	}
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
	if (!SettingEntry || !SettingEntry->IsEnabled() || !SettingEntry->IsVisible() || CapturePanel.IsValid())
	{
		return;
	}
	if(UWidgetPressAnyKeyPanelBase* Panel = UWidgetModuleStatics::CreateUserWidget<UWidgetPressAnyKeyPanelBase>())
	{
		CapturePanel = Panel;
		CaptureEntry = SettingEntry;
		Panel->OnKeySelected.AddUObject(this, &ThisClass::OnKeySelected, Panel);
		Panel->OnKeySelectionCanceled.AddUObject(this, &ThisClass::OnKeySelectionCanceled, Panel);
		Panel->Open(FWidgetPressAnyKeyOpenParameter());
	}
}

void UWidgetKeySettingEntryBase::OnKeySelected(FKey InKey, UWidgetPressAnyKeyPanelBase* InPanel)
{
	if (CapturePanel.Get() != InPanel || CaptureEntry.Get() != SettingEntry)
	{
		return;
	}
	ReleaseCapture(false);
	CommitUserValue(FParameter(InKey));
}

void UWidgetKeySettingEntryBase::OnKeySelectionCanceled(UWidgetPressAnyKeyPanelBase* InPanel)
{
	if (CapturePanel.Get() == InPanel)
	{
		ReleaseCapture(false);
	}
}

void UWidgetKeySettingEntryBase::ReleaseCapture(bool bClosePanel)
{
	UWidgetPressAnyKeyPanelBase* Panel = CapturePanel.Get();
	CapturePanel.Reset();
	CaptureEntry.Reset();
	if (Panel)
	{
		Panel->OnKeySelected.RemoveAll(this);
		Panel->OnKeySelectionCanceled.RemoveAll(this);
		if (bClosePanel)
		{
			Panel->Close(true);
		}
	}
}
