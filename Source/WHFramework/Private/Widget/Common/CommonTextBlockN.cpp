// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonTextBlockN.h"

UCommonTextBlockN::UCommonTextBlockN(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	static ConstructorHelpers::FClassFinder<UCommonTextStyle> StyleClassFinder(TEXT("/Script/Engine.Blueprint'/WHFramework/Widget/Blueprints/Common/_Style/Text/CTS_Default.CTS_Default_C'"));
	if(StyleClassFinder.Succeeded())
	{
		SetStyle(StyleClassFinder.Class);
	}
}

void UCommonTextBlockN::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if ( MyTextBlock.IsValid() )
	{
		MyTextBlock->SetHighlightText(PROPERTY_BINDING(FText, HighlightText));
		MyTextBlock->SetHighlightColor(HighlightColor.GetSpecifiedColor());
	}
}

void UCommonTextBlockN::OnBindingChanged(const FName& Property)
{
	Super::OnBindingChanged(Property);

	if ( MyTextBlock.IsValid() )
	{
		static const FName HighlightTextProperty(TEXT("HighlightTextDelegate"));
		static const FName HighlightColorProperty(TEXT("HighlightColorDelegate"));

		if ( Property == HighlightTextProperty )
		{
			MyTextBlock->SetText(PROPERTY_BINDING(FText, HighlightText));
		}
		else if ( Property == HighlightColorProperty )
		{
			MyTextBlock->SetColorAndOpacity(HighlightColor.GetSpecifiedColor());
		}
	}
}

void UCommonTextBlockN::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void UCommonTextBlockN::OnDespawn_Implementation(bool bRecovery)
{
	SetText(FText::GetEmpty());
	SetHighlightText(FText::GetEmpty());

	RemoveFromParent();
}

FText UCommonTextBlockN::GetHighlightText() const
{
	if (ColorAndOpacityDelegate.IsBound() && !IsDesignTime())
	{
		return HighlightTextDelegate.Execute();
	}
	return HighlightText;
}

void UCommonTextBlockN::SetHighlightText(const FText InText)
{
	HighlightText = InText;
	HighlightTextDelegate.Unbind();
	if (MyTextBlock.IsValid())
	{
		MyTextBlock->SetHighlightText(PROPERTY_BINDING(FText, HighlightText));
	}
}

FSlateColor UCommonTextBlockN::GetHighlightColor() const
{
	if (HighlightColorDelegate.IsBound() && !IsDesignTime())
	{
		return HighlightColorDelegate.Execute();
	}
	return HighlightColor;
}

void UCommonTextBlockN::SetHighlightColor(const FSlateColor InColor)
{
	HighlightColor = InColor;
	HighlightColorDelegate.Unbind();
	if (MyTextBlock.IsValid())
	{
		MyTextBlock->SetHighlightColor(InColor.GetSpecifiedColor());
	}
}
