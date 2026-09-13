#include "Setting/Widget/Entry/WidgetTextSettingEntryBase.h"

#include "Setting/SettingEntry.h"

UWidgetTextSettingEntryBase::UWidgetTextSettingEntryBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWidgetTextSettingEntryBase::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);
}

void UWidgetTextSettingEntryBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	if(TxtBox_Value)
	{
		TxtBox_Value->OnTextCommitted.RemoveDynamic(this, &ThisClass::OnTextCommitted);
	}
	Super::OnDespawn_Implementation(InMode);
}

void UWidgetTextSettingEntryBase::ApplyValueToControl_Implementation(const FParameter& InValue)
{
	if(!TxtBox_Value)
	{
		return;
	}
	TxtBox_Value->OnTextCommitted.RemoveDynamic(this, &ThisClass::OnTextCommitted);
	TxtBox_Value->OnTextCommitted.AddDynamic(this, &ThisClass::OnTextCommitted);

	FText Text;
	if(const int32* IntValue = InValue.GetPtr<int32>()) Text = FText::AsNumber(*IntValue);
	else if(const int64* Int64Value = InValue.GetPtr<int64>()) Text = FText::AsNumber(*Int64Value);
	else if(const float* FloatValue = InValue.GetPtr<float>()) Text = FText::AsNumber(*FloatValue);
	else if(const double* DoubleValue = InValue.GetPtr<double>()) Text = FText::AsNumber(*DoubleValue);
	else if(const FString* StringValue = InValue.GetPtr<FString>()) Text = FText::FromString(*StringValue);
	else if(const FName* NameValue = InValue.GetPtr<FName>()) Text = FText::FromName(*NameValue);
	else if(const FText* TextValue = InValue.GetPtr<FText>()) Text = *TextValue;
	TxtBox_Value->SetText(Text);
}

void UWidgetTextSettingEntryBase::OnTextCommitted(const FText& InText, ETextCommit::Type InCommitMethod)
{
	CommitUserValue(MakeTypedValue(InText.ToString()));
}

FParameter UWidgetTextSettingEntryBase::MakeTypedValue(const FString& InText) const
{
	const FParameter CurrentValue = SettingEntry ? SettingEntry->GetPendingValue() : FParameter();
	if(CurrentValue.Is<int32>()) return FParameter(FCString::Atoi(*InText));
	if(CurrentValue.Is<int64>()) return FParameter(FCString::Atoi64(*InText));
	if(CurrentValue.Is<float>()) return FParameter(FCString::Atof(*InText));
	if(CurrentValue.Is<double>()) return FParameter(FCString::Atod(*InText));
	if(CurrentValue.Is<FName>()) return FParameter(FName(*InText));
	if(CurrentValue.Is<FText>()) return FParameter(FText::FromString(InText));
	return FParameter(InText);
}
