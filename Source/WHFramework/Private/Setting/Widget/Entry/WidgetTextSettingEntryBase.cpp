#include "Setting/Widget/Entry/WidgetTextSettingEntryBase.h"

#include "Setting/SettingEntry.h"
#include "Widget/Common/CommonInput.h"

UWidgetTextSettingEntryBase::UWidgetTextSettingEntryBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

UWidget* UWidgetTextSettingEntryBase::GetValueControl() const
{
	return Input_Value;
}

FName UWidgetTextSettingEntryBase::GetRendererName() const
{
	return TEXT("Text");
}

void UWidgetTextSettingEntryBase::BindValueControl()
{
	if (Input_Value)
	{
		Input_Value->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::OnTextCommitted);
	}
}

void UWidgetTextSettingEntryBase::UnbindValueControl()
{
	if (Input_Value)
	{
		Input_Value->OnTextCommitted.RemoveDynamic(this, &ThisClass::OnTextCommitted);
	}
}

void UWidgetTextSettingEntryBase::ApplyValueToControl_Implementation(const FParameter& InValue)
{
	if (Input_Value)
	{
		Input_Value->SetValue(InValue);
	}
}

void UWidgetTextSettingEntryBase::OnTextCommitted(UCommonInput* InInput, const FText& InText, ETextCommit::Type InCommitMethod)
{
	FParameter Value;
	if (InCommitMethod != ETextCommit::OnCleared && InInput && InInput->TryGetValue(Value))
	{
		CommitUserValue(Value);
	}
	else
	{
		RefreshFromModel();
	}
}
