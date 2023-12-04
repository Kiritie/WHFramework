// Copyright Epic Games, Inc. All Rights Reserved.

#include "FSM/Customization/FSMComponentDetailCustomization.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "UObject/UnrealType.h"

#include "DetailLayoutBuilder.h"
#include "IDetailsView.h"

#include "ScopedTransaction.h"

FFSMComponentDetailCustomization::FFSMComponentDetailCustomization()
{
	
}

TSharedRef<IDetailCustomization> FFSMComponentDetailCustomization::MakeInstance()
{
	return MakeShared<FFSMComponentDetailCustomization>();
}

void FFSMComponentDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	FDetailCustomizationBase::CustomizeDetails(DetailLayoutBuilder);
}
