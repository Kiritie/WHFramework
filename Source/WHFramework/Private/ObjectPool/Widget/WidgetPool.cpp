// Fill out your copyright notice in the Description page of Project Settings.


#include "ObjectPool/Widget/WidgetPool.h"

#include "ObjectPool/ObjectPoolInterface.h"
#include "Blueprint/UserWidget.h"
#include "Debug/DebugModuleTypes.h"

UWidgetPool::UWidgetPool()
{
}

UObject* UWidgetPool::OnSpawn(UObject* InObject)
{
	if(!InObject)
	{
		InObject = CreateWidget<UUserWidget>(GetWorld(), Type.Get());
	}
	else if(InObject->IsRooted())
	{
		InObject->RemoveFromRoot();
	}
	return InObject;
}

void UWidgetPool::OnDespawn(UObject* InObject, bool bRecovery)
{
	Super::OnDespawn(InObject, bRecovery);
}
