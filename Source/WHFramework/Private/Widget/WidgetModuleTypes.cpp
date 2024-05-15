// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/WidgetModuleTypes.h"

FVector FWorldWidgetMapping::GetLocation() const
{
	return SceneComp ? (SocketName.IsNone() ? SceneComp->GetComponentLocation() : SceneComp->GetSocketLocation(SocketName)) + Location : Location;
}
