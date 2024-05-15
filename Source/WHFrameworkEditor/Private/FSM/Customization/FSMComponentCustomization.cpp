// Copyright Epic Games, Inc. All Rights Reserved.

#include "FSM/Customization/FSMComponentCustomization.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "UObject/UnrealType.h"

#include "DetailLayoutBuilder.h"
#include "IDetailsView.h"

#include "ScopedTransaction.h"

FFSMComponentCustomization::FFSMComponentCustomization()
{
	
}

TSharedRef<IDetailCustomization> FFSMComponentCustomization::MakeInstance()
{
	return MakeShared<FFSMComponentCustomization>();
}

void FFSMComponentCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	FClassCustomizationBase::CustomizeDetails(DetailLayoutBuilder);
}
