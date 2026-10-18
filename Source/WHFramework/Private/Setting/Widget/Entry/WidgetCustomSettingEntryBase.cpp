#include "Setting/Widget/Entry/WidgetCustomSettingEntryBase.h"

#include "Components/ContentWidget.h"
#include "Setting/SettingEntry.h"
#include "Setting/Widget/Entry/WidgetCustomSettingValueBase.h"
#include "Widget/Common/CommonTextBlockN.h"

UWidget* UWidgetCustomSettingEntryBase::GetValueControl() const
{
	return ValueControl;
}

FName UWidgetCustomSettingEntryBase::GetRendererName() const
{
	return TEXT("Custom");
}

void UWidgetCustomSettingEntryBase::ConfigureValueControl()
{
	if (!SettingEntry || !ValueHost) return;
	const TSubclassOf<UWidgetCustomSettingValueBase> Class = SettingEntry->GetDefinition().CustomWidgetClass;
	if (ValueControl && ValueControl->GetClass() != Class.Get())
	{
		ValueControl->RemoveFromParent();
		ValueControl = nullptr;
	}
	if (!ValueControl && Class)
	{
		ValueControl = CreateWidget<UWidgetCustomSettingValueBase>(GetOwningPlayer(), Class);
		ValueHost->SetContent(ValueControl);
	}
	if (ValueControl) ValueControl->Configure(SettingEntry->GetDefinition());
}

void UWidgetCustomSettingEntryBase::BindValueControl()
{
	if (ValueControl) ValueControl->OnValueChanged.AddUniqueDynamic(this, &ThisClass::OnValueChanged);
}

void UWidgetCustomSettingEntryBase::UnbindValueControl()
{
	if (ValueControl) ValueControl->OnValueChanged.RemoveDynamic(this, &ThisClass::OnValueChanged);
}

void UWidgetCustomSettingEntryBase::OnModelStateChanged()
{
	if (!SettingEntry) return;
	const bool bSupported = ValueControl && ValueControl->CanEditValue(SettingEntry->GetPendingValue());
	SetIsEnabledN(SettingEntry->IsEnabled() && bSupported);
	if (!bSupported)
	{
		const FText Reason = NSLOCTEXT("WH.Setting", "CustomRendererMissing", "此设置未配置支持当前值类型的自定义控件。");
		SetToolTipText(Reason);
		if (Txt_Reason)
		{
			Txt_Reason->SetText(Reason);
			Txt_Reason->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}
}

void UWidgetCustomSettingEntryBase::ApplyValueToControl_Implementation(const FParameter& InValue)
{
	if (ValueControl && ValueControl->CanEditValue(InValue)) ValueControl->PresentValue(InValue);
}

void UWidgetCustomSettingEntryBase::OnValueChanged(UWidgetCustomSettingValueBase* InControl, const FParameter& InValue)
{
	if (InControl == ValueControl) CommitUserValue(InValue);
}
