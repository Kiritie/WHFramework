// Fill out your copyright notice in the Description page of Project Settings.


#include "ObjectPool/Widget/WidgetPool.h"

#include "Blueprint/UserWidget.h"

UWidgetPool::UWidgetPool()
{
}

UObject* UWidgetPool::OnSpawn(UObject* InOwner, UObject* InObject)
{
	if(!InObject)
	{
		if(APlayerController* PlayerController = Cast<APlayerController>(InOwner))
		{
			InObject = CreateWidget(PlayerController, Type.Get());
		}
		else if(UGameInstance* GameInstance = Cast<UGameInstance>(InOwner))
		{
			InObject = CreateWidget(GameInstance, Type.Get());
		}
		else if(UWorld* World = Cast<UWorld>(InOwner))
		{
			if(!World->bIsTearingDown)
			{
				InObject = CreateWidget(World, Type.Get());
			}
		}
		else if(!GetWorld()->bIsTearingDown)
		{
			InObject = CreateWidget(GetWorld(), Type.Get());
		}
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
