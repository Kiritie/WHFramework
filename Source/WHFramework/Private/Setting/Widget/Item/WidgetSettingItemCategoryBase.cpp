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
}

void UWidgetSettingItemCategoryBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
	
	SetCategory(FText::GetEmpty());
}

void UWidgetSettingItemCategoryBase::OnCreate(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);
}

void UWidgetSettingItemCategoryBase::OnInitialize(const TArray<FParameter>& InParams)
{
	Super::OnInitialize(InParams);

	if(InParams.IsValidIndex(0))
	{
		SetCategory(InParams[0].GetTextValue());
	}
}

void UWidgetSettingItemCategoryBase::OnRefresh()
{
	Super::OnRefresh();
}

void UWidgetSettingItemCategoryBase::OnDestroy()
{
	Super::OnDestroy();
}

FText UWidgetSettingItemCategoryBase::GetCategory() const
{
	return Txt_Category->GetText();
}

void UWidgetSettingItemCategoryBase::SetCategory(FText InText)
{
	Txt_Category->SetText(InText);
}
