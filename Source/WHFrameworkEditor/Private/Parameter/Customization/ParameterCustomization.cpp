// Copyright Epic Games, Inc. All Rights Reserved.

#include "Parameter/Customization/ParameterCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "Parameter/ParameterTypes.h"

#define LOCTEXT_NAMESPACE "ParameterCustomization"

void FParameterCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedPtr<IPropertyHandle> ParameterDescriptionHandle = InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FParameter, Description)).ToSharedRef();

	FText ParameterDescription;
	ParameterDescriptionHandle->GetValue(ParameterDescription);

	FText ParameterPropertyName = InStructPropertyHandle->GetPropertyDisplayName();
	if(InStructPropertyHandle->GetParentHandle()->AsArray())
	{
		ParameterPropertyName = FText::FromString(FString::Printf(TEXT("%s [%d]"), *ParameterDescription.ToString(), InStructPropertyHandle->GetArrayIndex()));
	}

	HeaderRow
	.NameContent()
	[
		InStructPropertyHandle->CreatePropertyNameWidget(ParameterPropertyName, ParameterDescription)
	];
}

void FParameterCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
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
	
	auto ShowParameterValue = [this] (EParameterType ParameterType)
	{
		return TAttribute<EVisibility>::Create([this, ParameterType]()
		{
			uint8 ParameterTypeValue = 0;
			if (ParameterTypeHandle->GetValue(ParameterTypeValue) != FPropertyAccess::Success)
			{
				return EVisibility::Collapsed;
			}
			return static_cast<EParameterType>(ParameterTypeValue) == ParameterType ? EVisibility::Visible : EVisibility::Collapsed;
		});
	};

	ParameterTypeHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, ParameterType)).ToSharedRef();
	ParameterTypeHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FParameterCustomization::OnParameterTypeChanged));
	ChildBuilder.AddProperty(ParameterTypeHandle.ToSharedRef());

	DescriptionHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, Description)).ToSharedRef();
	ChildBuilder.AddProperty(DescriptionHandle.ToSharedRef());

	IntegerValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, IntegerValue)).ToSharedRef();
	ChildBuilder.AddProperty(IntegerValueHandle.ToSharedRef()).Visibility(ShowParameterValue(EParameterType::Integer));

	FloatValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, FloatValue)).ToSharedRef();
	ChildBuilder.AddProperty(FloatValueHandle.ToSharedRef()).Visibility(ShowParameterValue(EParameterType::Float));

	ByteValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, ByteValue)).ToSharedRef();
	ChildBuilder.AddProperty(ByteValueHandle.ToSharedRef()).Visibility(ShowParameterValue(EParameterType::Byte));

	EnumValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, EnumValue)).ToSharedRef();
	ChildBuilder.AddProperty(EnumValueHandle.ToSharedRef()).Visibility(ShowParameterValue(EParameterType::Enum));

	StringValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, StringValue)).ToSharedRef();
	ChildBuilder.AddProperty(StringValueHandle.ToSharedRef()).Visibility(ShowParameterValue(EParameterType::String));

	NameValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, NameValue)).ToSharedRef();
	ChildBuilder.AddProperty(NameValueHandle.ToSharedRef()).Visibility(ShowParameterValue(EParameterType::Name));

	TextValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, TextValue)).ToSharedRef();
	ChildBuilder.AddProperty(TextValueHandle.ToSharedRef()).Visibility(ShowParameterValue(EParameterType::Text));

	BooleanValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, BooleanValue)).ToSharedRef();
	ChildBuilder.AddProperty(BooleanValueHandle.ToSharedRef()).Visibility(ShowParameterValue(EParameterType::Boolean));

	VectorValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, VectorValue)).ToSharedRef();
	ChildBuilder.AddProperty(VectorValueHandle.ToSharedRef()).Visibility(ShowParameterValue(EParameterType::Vector));

	RotatorValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, RotatorValue)).ToSharedRef();
	ChildBuilder.AddProperty(RotatorValueHandle.ToSharedRef()).Visibility(ShowParameterValue(EParameterType::Rotator));

	ColorValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, ColorValue)).ToSharedRef();
	ChildBuilder.AddProperty(ColorValueHandle.ToSharedRef()).Visibility(ShowParameterValue(EParameterType::Color));

	KeyValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, KeyValue)).ToSharedRef();
	ChildBuilder.AddProperty(KeyValueHandle.ToSharedRef()).Visibility(ShowParameterValue(EParameterType::Key));

	TagValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, TagValue)).ToSharedRef();
	ChildBuilder.AddProperty(TagValueHandle.ToSharedRef()).Visibility(ShowParameterValue(EParameterType::Tag));

	TagsValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, TagsValue)).ToSharedRef();
	ChildBuilder.AddProperty(TagsValueHandle.ToSharedRef()).Visibility(ShowParameterValue(EParameterType::Tags));

	AssetIDValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, AssetIDValue)).ToSharedRef();
	ChildBuilder.AddProperty(AssetIDValueHandle.ToSharedRef()).Visibility(ShowParameterValue(EParameterType::AssetID));

	ClassValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, ClassValue)).ToSharedRef();
	ChildBuilder.AddProperty(ClassValueHandle.ToSharedRef()).Visibility(ShowParameterValue(EParameterType::Class));

	ClassPtrValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, ClassPtrValue)).ToSharedRef();
	ChildBuilder.AddProperty(ClassPtrValueHandle.ToSharedRef()).Visibility(ShowParameterValue(EParameterType::ClassPtr));

	ObjectValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, ObjectValue)).ToSharedRef();
	ChildBuilder.AddProperty(ObjectValueHandle.ToSharedRef()).Visibility(ShowParameterValue(EParameterType::Object));

	ObjectPtrValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, ObjectPtrValue)).ToSharedRef();
	ChildBuilder.AddProperty(ObjectPtrValueHandle.ToSharedRef()).Visibility(ShowParameterValue(EParameterType::ObjectPtr));

	DelegateValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, DelegateValue)).ToSharedRef();
	ChildBuilder.AddProperty(DelegateValueHandle.ToSharedRef()).Visibility(ShowParameterValue(EParameterType::Delegate));
}

void FParameterCustomization::OnParameterTypeChanged()
{
	IntegerValueHandle->ResetToDefault();
	FloatValueHandle->ResetToDefault();
	ByteValueHandle->ResetToDefault();
	EnumValueHandle->ResetToDefault();
	StringValueHandle->ResetToDefault();
	NameValueHandle->ResetToDefault();
	TextValueHandle->ResetToDefault();
	BooleanValueHandle->ResetToDefault();
	VectorValueHandle->ResetToDefault();
	RotatorValueHandle->ResetToDefault();
	ColorValueHandle->ResetToDefault();
	KeyValueHandle->ResetToDefault();
	TagValueHandle->ResetToDefault();
	TagsValueHandle->ResetToDefault();
	ClassValueHandle->ResetToDefault();
	ClassPtrValueHandle->ResetToDefault();
	ObjectValueHandle->ResetToDefault();
	ObjectPtrValueHandle->ResetToDefault();
	DelegateValueHandle->ResetToDefault();
}

#undef LOCTEXT_NAMESPACE
