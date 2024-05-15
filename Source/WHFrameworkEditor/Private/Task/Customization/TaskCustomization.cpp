// Copyright Epic Games, Inc. All Rights Reserved.

#include "Task/Customization/TaskCustomization.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "UObject/UnrealType.h"

#include "DetailLayoutBuilder.h"
#include "IDetailsView.h"

#include "ScopedTransaction.h"

FTaskCustomization::FTaskCustomization()
{
	
}

TSharedRef<IDetailCustomization> FTaskCustomization::MakeInstance()
{
	return MakeShared<FTaskCustomization>();
}

void FTaskCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	FClassCustomizationBase::CustomizeDetails(DetailLayoutBuilder);
}
