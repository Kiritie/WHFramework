#include "Widget/Common/CommonScroll.h"

#include "Widget/Style/CommonScrollStyle.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Widget/Theme/WidgetStyleTags.h"

UCommonScroll::UCommonScroll(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UCommonScroll::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (!ThemeChangedHandle.IsValid() && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		ThemeChangedHandle = FWidgetThemeResolver::OnThemeChanged().AddUObject(this, &ThisClass::ApplyStyleTag);
	}
	ApplyStyleTag();
}

void UCommonScroll::BeginDestroy()
{
	FWidgetThemeResolver::OnThemeChanged().Remove(ThemeChangedHandle);
	ThemeChangedHandle.Reset();
	Super::BeginDestroy();
}

FGameplayTag UCommonScroll::GetStyleTag() const
{
	if (bOverrideStyleTag && StyleTag.IsValid())
	{
		return StyleTag;
	}
	if (InheritedStyleTag.IsValid())
	{
		return InheritedStyleTag;
	}
	return StyleTag.IsValid() ? StyleTag : WidgetStyleTags::CommonDefault(EWidgetStyleKind::Scroll);
}

void UCommonScroll::SetStyleTag(FGameplayTag InStyleTag)
{
	StyleTag = InStyleTag;
	bOverrideStyleTag = InStyleTag.IsValid();
	ApplyStyleTag();
}

void UCommonScroll::SetInheritedStyleTag(FGameplayTag InStyleTag)
{
	InheritedStyleTag = InStyleTag;
	ApplyStyleTag();
}

void UCommonScroll::ClearStyleOverride()
{
	StyleTag = FGameplayTag();
	bOverrideStyleTag = false;
	ApplyStyleTag();
}

void UCommonScroll::ApplyStyleTag()
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}
	UClass* Class = FWidgetThemeResolver::ResolveClass(this, GetStyleTag(), EWidgetStyleKind::Scroll);
	const UCommonScrollStyle* Style = Class ? Class->GetDefaultObject<UCommonScrollStyle>() : nullptr;
	if (Style)
	{
		SetWidgetStyle(Style->ScrollBoxStyle);
		SetWidgetBarStyle(Style->ScrollBarStyle);
		SetScrollbarThickness(Style->ScrollBarThickness);
	}
}
