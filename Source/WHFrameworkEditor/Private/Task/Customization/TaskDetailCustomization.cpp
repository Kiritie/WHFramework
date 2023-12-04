// Copyright Epic Games, Inc. All Rights Reserved.

#include "Task/Customization/TaskDetailCustomization.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "UObject/UnrealType.h"

#include "DetailLayoutBuilder.h"
#include "IDetailsView.h"

#include "ScopedTransaction.h"

FTaskDetailCustomization::FTaskDetailCustomization()
{
	
}

TSharedRef<IDetailCustomization> FTaskDetailCustomization::MakeInstance()
{
	return MakeShared<FTaskDetailCustomization>();
}

void FTaskDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	FDetailCustomizationBase::CustomizeDetails(DetailLayoutBuilder);
}
