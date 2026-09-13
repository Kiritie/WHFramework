// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/WidgetModuleTypes.h"

#include "Widget/Screen/UserWidgetBase.h"
#include "Widget/World/WorldWidgetBase.h"

FGameplayTag FScreenWidgetConfig::ResolveWidgetTag() const
{
	if(WidgetTagOverride.IsValid())
	{
		return WidgetTagOverride;
	}

	if(const UUserWidgetBase* DefaultWidget = WidgetClass ? WidgetClass->GetDefaultObject<UUserWidgetBase>() : nullptr)
	{
		return DefaultWidget->GetDefaultWidgetTag();
	}

	return FGameplayTag();
}

FGameplayTag FScreenWidgetConfig::ResolveParentWidgetTag() const
{
	const FGameplayTag WidgetTag = ResolveWidgetTag();
	const FGameplayTag ScreenRootTag = FGameplayTag::RequestGameplayTag(TEXT("Widget.Screen"), false);
	if(!WidgetTag.IsValid() || !ScreenRootTag.IsValid() || !WidgetTag.MatchesTag(ScreenRootTag))
	{
		return FGameplayTag();
	}

	const FGameplayTag ParentWidgetTag = WidgetTag.RequestDirectParent();
	return ParentWidgetTag != ScreenRootTag ? ParentWidgetTag : FGameplayTag();
}

FGameplayTag FWorldWidgetConfig::ResolveWidgetTag() const
{
	if(WidgetTagOverride.IsValid())
	{
		return WidgetTagOverride;
	}
	if(const UWorldWidgetBase* DefaultObject = WidgetClass
		? WidgetClass->GetDefaultObject<UWorldWidgetBase>()
		: nullptr)
	{
		return DefaultObject->GetDefaultWidgetTag();
	}
	return FGameplayTag();
}

FVector FWorldWidgetMapping::GetLocation() const
{
	return SceneComp ? (SceneComp->IsVisible() ? (SocketName.IsNone() ? SceneComp->GetComponentLocation() : SceneComp->GetSocketLocation(SocketName)) + Location : FVector(-1.f)) : Location;
}
