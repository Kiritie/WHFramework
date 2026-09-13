#include "Widget/Common/CommonImageN.h"

#include "Widget/Theme/WidgetTheme.h"
#include "Widget/WidgetModule.h"

UCommonImageN::UCommonImageN(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UCommonImageN::SynchronizeProperties()
{
	ApplyStyleTag();
	Super::SynchronizeProperties();
}

void UCommonImageN::OnSpawn_Implementation(const FParameter& InParam)
{
	ApplyStyleTag();
}

void UCommonImageN::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
}

void UCommonImageN::SetStyleTag(FGameplayTag InStyleTag)
{
	StyleTag = InStyleTag;
	ApplyStyleTag();
}

void UCommonImageN::ApplyStyleTag()
{
	const bool bInEditor = IsDesignTime();
	const UWidgetModule* WidgetModule = UWidgetModule::IsValid(bInEditor)
		? UWidgetModule::GetPtr(bInEditor)
		: nullptr;
	const UWidgetTheme* Theme = WidgetModule ? WidgetModule->GetDefaultWidgetTheme() : nullptr;
	const FWidgetBrushStyleData* StyleData = Theme ? Theme->FindBrushStyle(StyleTag) : nullptr;
	if(StyleData)
	{
		SetBrush(StyleData->Brush);
	}
}
