// Fill out your copyright notice in the Description Item of Project Settings.

#include "Setting/Widget/Item/WidgetSettingItemCategoryBase.h"

#include "Components/TextBlock.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetSettingItemCategoryBase::UWidgetSettingItemCategoryBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UWidgetSettingItemCategoryBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
	
	if(InParams.IsValidIndex(0))
	{
		SetCategory(InParams[0]);
	}
}

void UWidgetSettingItemCategoryBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
	
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
