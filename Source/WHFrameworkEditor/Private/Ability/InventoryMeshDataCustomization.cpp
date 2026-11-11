#include "Ability/InventoryMeshDataCustomization.h"

#include "Ability/Inventory/InventoryItemPresentation.h"
#include "DetailWidgetRow.h"
#include "Engine/SkeletalMesh.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"

TSharedRef<IPropertyTypeCustomization> FInventoryMeshDataCustomization::MakeInstance()
{
	return MakeShared<FInventoryMeshDataCustomization>();
}

void FInventoryMeshDataCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InHandle, FDetailWidgetRow& InRow, IPropertyTypeCustomizationUtils& InUtils)
{
	InRow.NameContent()[InHandle->CreatePropertyNameWidget()];
}

void FInventoryMeshDataCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InHandle, IDetailChildrenBuilder& InBuilder, IPropertyTypeCustomizationUtils& InUtils)
{
	const TSharedRef<IPropertyHandle> Mesh = InHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FInventoryMeshData, Mesh)).ToSharedRef();
	InBuilder.AddProperty(Mesh);
	InBuilder.AddProperty(InHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FInventoryMeshData, AnimationClass)).ToSharedRef())
		.Visibility(TAttribute<EVisibility>::CreateLambda([Mesh]()
		{
			UObject* Value = nullptr;
			return Mesh->GetValue(Value) == FPropertyAccess::Success && Cast<USkeletalMesh>(Value)
				? EVisibility::Visible : EVisibility::Collapsed;
		}));
	InBuilder.AddProperty(InHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FInventoryMeshData, MaterialOverride)).ToSharedRef());
	InBuilder.AddProperty(InHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FInventoryMeshData, LocalTransform)).ToSharedRef());
}
