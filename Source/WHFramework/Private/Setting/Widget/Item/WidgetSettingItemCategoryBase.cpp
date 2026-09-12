// Fill out your copyright notice in the Description Item of Project Settings.

#include "Setting/Widget/Item/WidgetSettingItemCategoryBase.h"

#include "Components/TextBlock.h"
#include "Setting/SettingModuleTypes.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetSettingItemCategoryBase::UWidgetSettingItemCategoryBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UWidgetSettingItemCategoryBase::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);

	const FWidgetSettingItemCategorySpawnParameter& Parameter = InParam.GetRef<FWidgetSettingItemCategorySpawnParameter>();
	SetCategory(Parameter.Category);
}

void UWidgetSettingItemCategoryBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);
	
	SetCategory(FText::GetEmpty());
}

FText UWidgetSettingItemCategoryBase::GetCategory() const
{
	return Txt_Category->GetText();
}

void UWidgetSettingItemCategoryBase::SetCategory(FText InText)
{
	Txt_Category->SetText(InText);
}
