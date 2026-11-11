#include "Ability/AbilityLevelValueCustomization.h"

#include "Ability/AbilityLevelValue.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IPropertyTypeCustomization> FAbilityLevelValueCustomization::MakeInstance()
{
	return MakeShared<FAbilityLevelValueCustomization>();
}

void FAbilityLevelValueCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InHandle, FDetailWidgetRow& InRow, IPropertyTypeCustomizationUtils& InUtils)
{
	Handle = InHandle;
	InRow.NameContent()[InHandle->CreatePropertyNameWidget()]
	.ValueContent().MinDesiredWidth(160.f)[InHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAbilityLevelValue, Base))->CreatePropertyValueWidget()];
}

void FAbilityLevelValueCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InHandle, IDetailChildrenBuilder& InBuilder, IPropertyTypeCustomizationUtils& InUtils)
{
	InBuilder.AddProperty(InHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAbilityLevelValue, Growth)).ToSharedRef());
	InBuilder.AddProperty(InHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAbilityLevelValue, Increment)).ToSharedRef());
	InBuilder.AddProperty(InHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAbilityLevelValue, Multiplier)).ToSharedRef());
	InBuilder.AddCustomRow(NSLOCTEXT("AbilityLevelValue", "PreviewLevel", "预览等级"))
	.NameContent()[SNew(STextBlock).Text(NSLOCTEXT("AbilityLevelValue", "PreviewLevel", "预览等级"))]
	.ValueContent()[SNew(SNumericEntryBox<int32>).MinValue(1).MaxValue(1000000)
		.Value_Lambda([this]() -> TOptional<int32> { return PreviewLevel; })
		.OnValueChanged_Lambda([this](int32 InValue) { PreviewLevel = FMath::Clamp(InValue, 1, 1000000); })];
	InBuilder.AddCustomRow(NSLOCTEXT("AbilityLevelValue", "Result", "计算结果"))
	.NameContent()[SNew(STextBlock).Text(NSLOCTEXT("AbilityLevelValue", "Result", "计算结果"))]
	.ValueContent()[SNew(STextBlock).Text_Lambda([this]() { return GetPreview(); })];
}

FText FAbilityLevelValueCustomization::GetPreview() const
{
	FAbilityLevelValue Value;
	uint8 Growth = 0;
	if (!Handle.IsValid() ||
		Handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAbilityLevelValue, Base))->GetValue(Value.Base) != FPropertyAccess::Success ||
		Handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAbilityLevelValue, Growth))->GetValue(Growth) != FPropertyAccess::Success)
	{
		return NSLOCTEXT("AbilityLevelValue", "Multiple", "多个值");
	}
	Value.Growth = static_cast<EAbilityValueGrowth>(Growth);
	if ((Value.Growth == EAbilityValueGrowth::Linear && Handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAbilityLevelValue, Increment))->GetValue(Value.Increment) != FPropertyAccess::Success) ||
		(Value.Growth == EAbilityValueGrowth::Exponential && Handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAbilityLevelValue, Multiplier))->GetValue(Value.Multiplier) != FPropertyAccess::Success))
	{
		return NSLOCTEXT("AbilityLevelValue", "Multiple", "多个值");
	}
	return Value.IsValid(PreviewLevel) ? FText::AsNumber(Value.Evaluate(PreviewLevel)) : NSLOCTEXT("AbilityLevelValue", "Invalid", "参数无效或数值溢出");
}
