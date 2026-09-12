// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/WidgetModuleTypes.h"

#include "Widget/Screen/UserWidgetBase.h"
#include "Widget/World/WorldWidgetBase.h"

FGameplayTag FScreenWidgetConfig::ResolveWidgetTag() const
{
	return WidgetTagOverride;
}

FGameplayTag FWorldWidgetConfig::ResolveWidgetTag() const
{
	return WidgetTagOverride;
}

FVector FWorldWidgetMapping::GetLocation() const
{
	return SceneComp ? (SceneComp->IsVisible() ? (SocketName.IsNone() ? SceneComp->GetComponentLocation() : SceneComp->GetSocketLocation(SocketName)) + Location : FVector(-1.f)) : Location;
}
