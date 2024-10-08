// Copyright Epic Games, Inc. All Rights Reserved.

#include "Parameter/Customization/Value/EnumParameterValueCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "Parameter/ParameterTypes.h"

#define LOCTEXT_NAMESPACE "EnumValueCustomization"

void FEnumParameterValueCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// HeaderRow
	// .NameContent()
	// [
	// 	InStructPropertyHandle->CreatePropertyNameWidget()
	// ];
}

void FEnumParameterValueCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TMap<FName, TSharedPtr<IPropertyHandle>> PropertyHandles;
	uint32 NumChildren;
	InStructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = InStructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
		const FName PropertyName = ChildHandle->GetProperty()->GetFName();
		PropertyHandles.Add(PropertyName, ChildHandle);
	}
	
	EnumTypeHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FEnumParameterValue, EnumType));
	EnumTypeHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FEnumParameterValueCustomization::OnEnumTypeChanged, true));
	ChildBuilder.AddProperty(EnumTypeHandle.ToSharedRef());

	EnumNameHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FEnumParameterValue, EnumName));
	EnumNameHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FEnumParameterValueCustomization::OnEnumTypeChanged, true));
	ChildBuilder.AddProperty(EnumNameHandle.ToSharedRef())
		.Visibility(
			TAttribute<EVisibility>::Create([this]()
			{
				UObject* EnumTypeValue = nullptr;
				if (EnumTypeHandle->GetValue(EnumTypeValue) != FPropertyAccess::Success)
				{
					return EVisibility::Collapsed;
				}
				return EnumTypeValue ? EVisibility::Collapsed : EVisibility::Visible;
			})
		);

	EnumNamesHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FEnumParameterValue, EnumNames));
	EnumNamesHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FEnumParameterValueCustomization::OnEnumTypeChanged, true));
	ChildBuilder.AddProperty(EnumNamesHandle.ToSharedRef())
		.Visibility(
			TAttribute<EVisibility>::Create([this]()
			{
				UObject* EnumTypeValue = nullptr;
				if (EnumTypeHandle->GetValue(EnumTypeValue) != FPropertyAccess::Success)
				{
					return EVisibility::Collapsed;
				}
				return EnumTypeValue ? EVisibility::Collapsed : EVisibility::Visible;
			})
		);

	EnumValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FEnumParameterValue, EnumValue));

	ChildBuilder.AddProperty(EnumValueHandle.ToSharedRef())
		.Visibility(
			TAttribute<EVisibility>::Create([this]()
			{
				return EnumPropValues.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
			})
		)
		.CustomWidget()
			.NameContent()
			[
				EnumValueHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			[
				SNew(SComboButton)
				.OnGetMenuContent(this, &FEnumParameterValueCustomization::OnGetEnumValueContent)
				.ContentPadding(FMargin( 2.0f, 2.0f ))
				.ButtonContent()
				[
					SNew(STextBlock) 
					.Text(this, &FEnumParameterValueCustomization::GetCurrentEnumValueDesc)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			];

	OnEnumTypeChanged();
}

void FEnumParameterValueCustomization::PreChange(const UUserDefinedEnum* Changed, FEnumEditorUtils::EEnumEditorChangeInfo ChangedType)
{
	// Implementing interface pure virtual method but nothing to do here
}

void FEnumParameterValueCustomization::PostChange(const UUserDefinedEnum* Changed, FEnumEditorUtils::EEnumEditorChangeInfo ChangedType)
{
	OnEnumTypeChanged(false);
}

void FEnumParameterValueCustomization::OnEnumTypeChanged(bool bPropertyChanged)
{
	EnumPropValues.Empty();

	UObject* EnumTypeValue;
	EnumTypeHandle->GetValue(EnumTypeValue);

	FString EnumNameValue;
	EnumNameHandle->GetValue(EnumNameValue);

	TArray<FString> EnumNamesValue;
	EnumNameHandle->GetValue(EnumNameValue);

	if(bPropertyChanged)
	{
		EnumValueHandle->SetValue(0);
	}

	UEnum* SelectedEnumType = nullptr;

	if (EnumTypeValue)
	{
		SelectedEnumType = Cast<UEnum>(EnumTypeValue);
	}
	else if(!EnumNameValue.IsEmpty())
	{
		SelectedEnumType = LoadObject<UEnum>(nullptr, *EnumNameValue);
	}

	if (SelectedEnumType)
	{
		for (int32 i = 0; i < SelectedEnumType->NumEnums() - 1; i++)
		{
			FString DisplayedName = SelectedEnumType->GetDisplayNameTextByIndex(i).ToString();
			EnumPropValues.Add(DisplayedName);
		}
	}
	else if(EnumNamesValue.Num() > 0)
	{
		EnumPropValues = EnumNamesValue;
	}
}

TSharedRef<SWidget> FEnumParameterValueCustomization::OnGetEnumValueContent() const
{
	FMenuBuilder MenuBuilder(true, NULL);

	for (uint8 i = 0; i < EnumPropValues.Num(); i++)
	{
		FUIAction ItemAction( FExecuteAction::CreateSP( const_cast<FEnumParameterValueCustomization*>(this), &FEnumParameterValueCustomization::OnEnumValueComboChange, i ) );
		MenuBuilder.AddMenuEntry( FText::FromString( EnumPropValues[i] ), TAttribute<FText>(), FSlateIcon(), ItemAction);
	}

	return MenuBuilder.MakeWidget();
}

FText FEnumParameterValueCustomization::GetCurrentEnumValueDesc() const
{
	FPropertyAccess::Result Result = FPropertyAccess::Fail;
	uint8 EnumIndex = INDEX_NONE;

	if (EnumPropValues.Num() > 0)
	{	
		Result = EnumValueHandle->GetValue(EnumIndex);
	}

	return (Result == FPropertyAccess::Success && EnumPropValues.IsValidIndex(EnumIndex))
		? FText::FromString(EnumPropValues[EnumIndex])
		: FText::GetEmpty();
}

void FEnumParameterValueCustomization::OnEnumValueComboChange(uint8 Index)
{
	if (EnumPropValues.Num() > 0)
	{
		EnumValueHandle->SetValue(Index);
	}
}


#undef LOCTEXT_NAMESPACE
