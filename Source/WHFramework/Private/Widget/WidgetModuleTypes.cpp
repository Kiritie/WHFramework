// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/WidgetModuleTypes.h"

FVector FWorldWidgetMapping::GetLocation() const
{
	return SceneComp ? (SceneComp->IsVisible() ? (SocketName.IsNone() ? SceneComp->GetComponentLocation() : SceneComp->GetSocketLocation(SocketName)) + Location : FVector(-1.f)) : Location;
}
