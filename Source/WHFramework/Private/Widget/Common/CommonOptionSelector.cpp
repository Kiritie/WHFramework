#include "Widget/Common/CommonOptionSelector.h"

#include "Components/EditableTextBox.h"
#include "Widget/WidgetModuleTypes.h"

UCommonOptionSelector::UCommonOptionSelector(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UCommonOptionSelector::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);
	if(const FCommonOptionSelectorSpawnParameter* Param = InParam.GetPtr<FCommonOptionSelectorSpawnParameter>())
	{
		OptionNames = Param->Options;
		bEditable = Param->bEditable;
		SetTitle(Param->Title);
	}

	if(Btn_Last)
	{
		Btn_Last->SetStyleTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Style.Button.Setting.Previous")), false));
		Btn_Last->OnClicked().RemoveAll(this);
		Btn_Last->OnClicked().AddUObject(this, &ThisClass::OnPreviousClicked);
	}
	if(Btn_Next)
	{
		Btn_Next->SetStyleTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Style.Button.Setting.Next")), false));
		Btn_Next->OnClicked().RemoveAll(this);
		Btn_Next->OnClicked().AddUObject(this, &ThisClass::OnNextClicked);
	}
	if(TxtBox_Value)
	{
		TxtBox_Value->OnTextChanged.RemoveDynamic(this, &ThisClass::OnTextValueChanged);
		TxtBox_Value->OnTextChanged.AddDynamic(this, &ThisClass::OnTextValueChanged);
		TxtBox_Value->SetIsReadOnly(!bEditable);
	}
	RefreshValue();
}

void UCommonOptionSelector::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	if(Btn_Last)
	{
		Btn_Last->OnClicked().RemoveAll(this);
	}
	if(Btn_Next)
	{
		Btn_Next->OnClicked().RemoveAll(this);
	}
	if(TxtBox_Value)
	{
		TxtBox_Value->OnTextChanged.RemoveDynamic(this, &ThisClass::OnTextValueChanged);
	}
	OnValueChanged.Clear();
	OptionNames.Reset();
	OptionIndex = 0;
	SetTitle(FText::GetEmpty());

	Super::OnDespawn_Implementation(InMode);
}

void UCommonOptionSelector::OnTextValueChanged(const FText& InText)
{
	if(!bEditable)
	{
		return;
	}

	const int32 NewIndex = OptionNames.IndexOfByKey(InText.ToString());
	if(NewIndex != INDEX_NONE && NewIndex != OptionIndex)
	{
		OptionIndex = NewIndex;
		OnValueChanged.Broadcast(this, GetValue());
	}
}

void UCommonOptionSelector::OnPreviousClicked()
{
	SetOptionIndex(OptionIndex - 1);
}

void UCommonOptionSelector::OnNextClicked()
{
	SetOptionIndex(OptionIndex + 1);
}

void UCommonOptionSelector::RefreshValue()
{
	OptionIndex = OptionNames.IsValidIndex(OptionIndex) ? OptionIndex : 0;
	if(TxtBox_Value)
	{
		TxtBox_Value->SetText(
			OptionNames.IsValidIndex(OptionIndex)
				? FText::FromString(OptionNames[OptionIndex])
				: FText::GetEmpty());
	}
	if(Btn_Last)
	{
		Btn_Last->SetIsEnabled(OptionIndex > 0);
	}
	if(Btn_Next)
	{
		Btn_Next->SetIsEnabled(OptionNames.IsValidIndex(OptionIndex + 1));
	}
}

void UCommonOptionSelector::SetOptionNames(const TArray<FString>& InOptionNames)
{
	OptionNames = InOptionNames;
	OptionIndex = 0;
	RefreshValue();
}

FString UCommonOptionSelector::GetSelectedOption() const
{
	return OptionNames.IsValidIndex(OptionIndex) ? OptionNames[OptionIndex] : FString();
}

FParameter UCommonOptionSelector::GetValue() const
{
	return GetSelectedOption();
}

void UCommonOptionSelector::SetValue(const FParameter& InValue)
{
	if(const FString* Value = InValue.GetPtr<FString>())
	{
		SetOptionIndex(OptionNames.IndexOfByKey(*Value));
	}
}

void UCommonOptionSelector::SetOptionIndex(int32 InOptionIndex)
{
	if(!OptionNames.IsValidIndex(InOptionIndex) || InOptionIndex == OptionIndex)
	{
		return;
	}

	OptionIndex = InOptionIndex;
	RefreshValue();
	OnValueChanged.Broadcast(this, GetValue());
}
