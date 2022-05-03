// Fill out your copyright notice in the Description page of Project Settings.


#include "ObjectPool/Widget/WidgetPool.h"

#include "ObjectPool/ObjectPoolInterface.h"
#include "Blueprint/UserWidget.h"

UWidgetPool::UWidgetPool()
{
}

UObject* UWidgetPool::SpawnImpl()
{
	UObject* Object = CreateWidget<UUserWidget>(GetWorld(), Type.Get());
	return Object;
}

void UWidgetPool::DespawnImpl(UObject* InObject)
{
	Super::DespawnImpl(InObject);
}
