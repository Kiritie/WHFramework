// Copyright Epic Games, Inc. All Rights Reserved.

#include "Procedure/Customization/ProcedureCustomization.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "UObject/UnrealType.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "IDetailGroup.h"
#include "IDetailsView.h"

#include "ScopedTransaction.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Windows/WindowsPlatformApplicationMisc.h"

FProcedureCustomization::FProcedureCustomization()
{
	
}

TSharedRef<IDetailCustomization> FProcedureCustomization::MakeInstance()
{
	return MakeShared<FProcedureCustomization>();
}

void FProcedureCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	FClassCustomizationBase::CustomizeDetails(DetailLayoutBuilder);
	
	IDetailCategoryBuilder& OperationTargetCategory = DetailLayoutBuilder.EditCategory(FName("Operation Target"));
	
	OperationTargetCategory.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, OperationTarget)));
	OperationTargetCategory.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, bApplyCameraAction)));
	OperationTargetCategory.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, CameraModeClass)));
	OperationTargetCategory.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, CameraOverride)));
	OperationTargetCategory.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, CameraAnchor)));
	OperationTargetCategory.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UProcedureBase, CameraTransition)));
}
