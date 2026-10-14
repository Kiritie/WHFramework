#include "Widget/Common/CommonImageN.h"

#include "Widget/Theme/WidgetTheme.h"
#include "Widget/Theme/WidgetThemeResolver.h"

UCommonImageN::UCommonImageN(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}

void UCommonImageN::SynchronizeProperties()
{
	EnsureThemeSubscription();
	ApplyStyleTag();
	Super::SynchronizeProperties();
}

void UCommonImageN::OnSpawn_Implementation(const FParameter& InParam)
{
	EnsureThemeSubscription();
	ApplyStyleTag();
}

void UCommonImageN::OnDespawn_Implementation(EObjectDespawnMode InMode) {}

void UCommonImageN::SetStyleTag(FGameplayTag InStyleTag)
{
	StyleTag = InStyleTag;
	ApplyStyleTag();
}

void UCommonImageN::ApplyStyleTag()
{
	if (!StyleTag.IsValid())
	{
		return;
	}
	EnsureThemeSubscription();
	UClass* Class = FWidgetThemeResolver::ResolveClass(this, StyleTag, EWidgetStyleKind::Brush);
	const UWidgetBrushStyle* Style = Class ? Class->GetDefaultObject<UWidgetBrushStyle>() : nullptr;
	SetBrush(Style ? Style->Brush : FSlateBrush());
}

void UCommonImageN::EnsureThemeSubscription()
{
	if (!ThemeChangedHandle.IsValid() && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		ThemeChangedHandle = FWidgetThemeResolver::OnThemeChanged().AddUObject(this, &ThisClass::ApplyStyleTag);
	}
}
void UCommonImageN::BeginDestroy()
{
	FWidgetThemeResolver::OnThemeChanged().Remove(ThemeChangedHandle);
	ThemeChangedHandle.Reset();
	Super::BeginDestroy();
}
