// Copyright Epic Games, Inc. All Rights Reserved.

#include "Parameter/Customization/ParameterCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "Parameter/ParameterTypes.h"

#define LOCTEXT_NAMESPACE "ParameterCustomization"

void FParameterCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedPtr<IPropertyHandle> ParameterDescriptionHandle = InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FParameter, Description));

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
		TSharedPtr<IPropertyHandle> ChildHandle = InStructPropertyHandle->GetChildHandle(ChildIndex);
		const FName PropertyName = ChildHandle->GetProperty()->GetFName();
		PropertyHandles.Add(PropertyName, ChildHandle);
	}

	ParameterTypeHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, ParameterType));
	ParameterTypeHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FParameterCustomization::OnParameterTypeChanged));
	ChildBuilder.AddProperty(ParameterTypeHandle.ToSharedRef());

	DescriptionHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, Description));
	ChildBuilder.AddProperty(DescriptionHandle.ToSharedRef());
	
#define ADD_PARAM_PROPERTY(ParamType) \
	ParamType##ValueHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FParameter, ParamType##Value)); \
	ChildBuilder.AddProperty(ParamType##ValueHandle.ToSharedRef()).Visibility(TAttribute<EVisibility>::Create([this]() \
		{ \
			uint8 ParamTypeValue = 0; \
			return ParameterTypeHandle->GetValue(ParamTypeValue) == FPropertyAccess::Success && (EParameterType)ParamTypeValue == EParameterType::ParamType ? EVisibility::Visible : EVisibility::Collapsed; \
		}) \
	);

	ADD_PARAM_PROPERTY(Integer)
	ADD_PARAM_PROPERTY(Float)
	ADD_PARAM_PROPERTY(Byte)
	ADD_PARAM_PROPERTY(Enum)
	ADD_PARAM_PROPERTY(String)
	ADD_PARAM_PROPERTY(Name)
	ADD_PARAM_PROPERTY(Text)
	ADD_PARAM_PROPERTY(Boolean)
	ADD_PARAM_PROPERTY(Vector)
	ADD_PARAM_PROPERTY(Rotator)
	ADD_PARAM_PROPERTY(Color)
	ADD_PARAM_PROPERTY(Key)
	ADD_PARAM_PROPERTY(Tag)
	ADD_PARAM_PROPERTY(Tags)
	ADD_PARAM_PROPERTY(Brush)
	ADD_PARAM_PROPERTY(Class)
	ADD_PARAM_PROPERTY(ClassPtr)
	ADD_PARAM_PROPERTY(Object)
	ADD_PARAM_PROPERTY(ObjectInst)
	ADD_PARAM_PROPERTY(ObjectPtr)
	ADD_PARAM_PROPERTY(Delegate)
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
