// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonTextBlockN.h"

#include "Widget/Theme/WidgetTheme.h"
#include "Widget/Theme/WidgetThemeResolver.h"

UCommonTextBlockN::UCommonTextBlockN(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}

void UCommonTextBlockN::SynchronizeProperties()
{
	EnsureThemeSubscription();
	if (StyleTag.IsValid() && !bApplyingStyleTag)
	{
		ApplyStyleTag();
	}
	Super::SynchronizeProperties();

	if (MyTextBlock.IsValid())
	{
		MyTextBlock->SetHighlightText(PROPERTY_BINDING(FText, HighlightText));
		MyTextBlock->SetHighlightColor(HighlightColor.GetSpecifiedColor());
	}
}

void UCommonTextBlockN::OnBindingChanged(const FName& Property)
{
	Super::OnBindingChanged(Property);

	if (MyTextBlock.IsValid())
	{
		static const FName HighlightTextProperty(TEXT("HighlightTextDelegate"));
		static const FName HighlightColorProperty(TEXT("HighlightColorDelegate"));

		if (Property == HighlightTextProperty)
		{
			MyTextBlock->SetText(PROPERTY_BINDING(FText, HighlightText));
		}
		else if (Property == HighlightColorProperty)
		{
			MyTextBlock->SetColorAndOpacity(HighlightColor.GetSpecifiedColor());
		}
	}
}

void UCommonTextBlockN::OnSpawn_Implementation(const FParameter& InParam)
{
	EnsureThemeSubscription();
	ApplyStyleTag();
}

void UCommonTextBlockN::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	SetText(FText::GetEmpty());
	SetHighlightText(FText::GetEmpty());
}

FText UCommonTextBlockN::GetHighlightText() const
{
	if (HighlightTextDelegate.IsBound() && !IsDesignTime())
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

void UCommonTextBlockN::SetStyleTag(FGameplayTag InStyleTag)
{
	StyleTag = InStyleTag;
	ApplyStyleTag();
}

void UCommonTextBlockN::ApplyStyleTag()
{
	if (!StyleTag.IsValid())
	{
		return;
	}
	EnsureThemeSubscription();
	if (bApplyingStyleTag)
	{
		return;
	}
	TGuardValue<bool> Guard(bApplyingStyleTag, true);
	SetStyle(FWidgetThemeResolver::ResolveClass(this, StyleTag, EWidgetStyleKind::Text));
}

void UCommonTextBlockN::EnsureThemeSubscription()
{
	if (!ThemeChangedHandle.IsValid() && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		ThemeChangedHandle = FWidgetThemeResolver::OnThemeChanged().AddUObject(this, &ThisClass::ApplyStyleTag);
	}
}
void UCommonTextBlockN::BeginDestroy()
{
	FWidgetThemeResolver::OnThemeChanged().Remove(ThemeChangedHandle);
	ThemeChangedHandle.Reset();
	Super::BeginDestroy();
}
