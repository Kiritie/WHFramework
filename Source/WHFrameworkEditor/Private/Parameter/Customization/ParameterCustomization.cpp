#include "Parameter/Customization/ParameterCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"

void FParameterCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructHandle, FDetailWidgetRow& HeaderRow,
											  IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedPtr<IPropertyHandle> DescriptionHandle = StructHandle->GetChildHandle(TEXT("Description"));
	FText Description;
	if (DescriptionHandle)
		DescriptionHandle->GetValue(Description);
	FText DisplayName = StructHandle->GetPropertyDisplayName();
	if (StructHandle->GetParentHandle()->AsArray())
	{
		DisplayName = FText::FromString(FString::Printf(TEXT("%s [%d]"), *Description.ToString(), StructHandle->GetArrayIndex()));
	}
	HeaderRow.NameContent()[StructHandle->CreatePropertyNameWidget(DisplayName, Description)];
}

void FParameterCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructHandle, IDetailChildrenBuilder& ChildBuilder,
												IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if (TSharedPtr<IPropertyHandle> DescriptionHandle = StructHandle->GetChildHandle(TEXT("Description")))
	{
		ChildBuilder.AddProperty(DescriptionHandle.ToSharedRef());
	}
	if (TSharedPtr<IPropertyHandle> ValueHandle = StructHandle->GetChildHandle(TEXT("Value")))
	{
		ChildBuilder.AddProperty(ValueHandle.ToSharedRef());
	}
}
