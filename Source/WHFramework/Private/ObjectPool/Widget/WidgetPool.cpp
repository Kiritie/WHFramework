// Fill out your copyright notice in the Description page of Project Settings.


#include "ObjectPool/Widget/WidgetPool.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"

UWidgetPool::UWidgetPool()
{
}

UObject* UWidgetPool::OnSpawn(UObject* InOwner, UObject* InObject)
{
	UWidget* OwningWidget = Cast<UWidget>(InOwner);
	UWidgetTree* OwningWidgetTree = nullptr;
	if(UUserWidget* OwningUserWidget = Cast<UUserWidget>(OwningWidget))
	{
		OwningWidgetTree = OwningUserWidget->WidgetTree;
	}
	else if(OwningWidget)
	{
		OwningWidgetTree = OwningWidget->GetTypedOuter<UWidgetTree>();
	}
	UWorld* OwningWorld = InOwner ? InOwner->GetWorld() : GetWorld();
	if(UUserWidget* PooledWidget = Cast<UUserWidget>(InObject))
	{
		if(!PooledWidget->WidgetTree || (OwningWorld && PooledWidget->GetWorld() != OwningWorld) || (OwningWidgetTree && PooledWidget->GetOuter() != OwningWidgetTree))
		{
			if(PooledWidget->IsRooted())
			{
				PooledWidget->RemoveFromRoot();
			}
			InObject = nullptr;
		}
	}
	if(!InObject)
	{
		if(OwningWidget)
		{
			InObject = CreateWidget<UUserWidget>(OwningWidget, Type.Get());
		}
		else if(APlayerController* PlayerController = Cast<APlayerController>(InOwner))
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
		else if(OwningWorld && !OwningWorld->bIsTearingDown)
		{
			InObject = CreateWidget(OwningWorld, Type.Get());
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
