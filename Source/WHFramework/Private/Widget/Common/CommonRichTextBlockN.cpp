// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonRichTextBlockN.h"

#include "Widget/WidgetModuleStatics.h"
#include "CommonTextBlock.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Widget/Theme/WidgetStyleTags.h"
#include "CommonTextScrollStyleBinding.h"

UCommonRichTextBlockN::UCommonRichTextBlockN(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bUseCommonRichStyle = false;
	StyleTag = WidgetStyleTags::CommonDefault(EWidgetStyleKind::Text);
	TextScrollStyleTag = WidgetStyleTags::CommonDefault(EWidgetStyleKind::TextScroll);
}

void UCommonRichTextBlockN::OnSpawn_Implementation(const FParameter& InParam)
{
	RefreshRichStyle();
}

TSharedRef<SWidget> UCommonRichTextBlockN::RebuildWidget()
{
	TextScrollBinding = MakeShared<FCommonTextScrollStyleBinding>();
	ApplyStyleTags();
	return TextScrollBinding->Build([this] { return Super::RebuildWidget(); });
}

void UCommonRichTextBlockN::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	TextScrollBinding.Reset();
}

void UCommonRichTextBlockN::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	SetText(FText::GetEmpty());
}

void UCommonRichTextBlockN::SetText(const FText& InText)
{
	Super::SetText(InText);

	RefreshRichStyle();
}

void UCommonRichTextBlockN::RefreshRichStyle()
{
	if(bUseCommonRichStyle)
	{
		SetTextStyleSet(UWidgetModuleStatics::GetCommonRichTextStyle());
		SetDecorators(UWidgetModuleStatics::GetCommonRichTextDecorators());
	}
	ApplyStyleTags();
}

void UCommonRichTextBlockN::SynchronizeProperties()
{
	ApplyStyleTags();
	Super::SynchronizeProperties();
}

void UCommonRichTextBlockN::ApplyStyleTags()
{
	if (bApplyingStyleTags || HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}
	TGuardValue<bool> Guard(bApplyingStyleTags, true);
	if (!ThemeChangedHandle.IsValid())
	{
		ThemeChangedHandle = FWidgetThemeResolver::OnThemeChanged().AddUObject(this, &ThisClass::ApplyStyleTags);
	}
	if (StyleTag.IsValid())
	{
		UClass* Class = FWidgetThemeResolver::ResolveClass(this, StyleTag, EWidgetStyleKind::Text);
		if (const UCommonTextStyle* Style = Class ? Class->GetDefaultObject<UCommonTextStyle>() : nullptr)
		{
			SetStyle(Class);
		}
	}
	if (TextScrollStyleTag.IsValid())
	{
		if (!TextScrollBinding)
		{
			TextScrollBinding = MakeShared<FCommonTextScrollStyleBinding>();
		}
		TextScrollBinding->Apply(this, FWidgetThemeResolver::ResolveClass(this, TextScrollStyleTag, EWidgetStyleKind::TextScroll),
			[this] { return Super::RebuildWidget(); });
	}
}

void UCommonRichTextBlockN::SetStyleTag(FGameplayTag InStyleTag)
{
	StyleTag = InStyleTag;
	ApplyStyleTags();
	Super::SynchronizeProperties();
}

void UCommonRichTextBlockN::SetTextScrollStyleTag(FGameplayTag InStyleTag)
{
	TextScrollStyleTag = InStyleTag;
	if (!InStyleTag.IsValid())
	{
		TGuardValue<bool> Guard(bApplyingStyleTags, true);
		if (TextScrollBinding)
		{
			TextScrollBinding->Apply(this, nullptr, [this] { return Super::RebuildWidget(); });
		}
	}
	ApplyStyleTags();
	Super::SynchronizeProperties();
}

void UCommonRichTextBlockN::BeginDestroy()
{
	FWidgetThemeResolver::OnThemeChanged().Remove(ThemeChangedHandle);
	ThemeChangedHandle.Reset();
	Super::BeginDestroy();
}
