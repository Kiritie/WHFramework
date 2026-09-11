#include "ObjectPool/Provider/WidgetPoolProvider.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "ObjectPool/ObjectPoolBucket.h"
#include "ObjectPool/ObjectPoolModule.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"

FWidgetPoolProvider::FWidgetPoolProvider(UObjectPoolModule& InModule)
	: Module(InModule)
{
}

UObject* FWidgetPoolProvider::Spawn(UClass* InClass, const FParameter& InParameter)
{
	const FWidgetSpawnParameter* Parameter = InParameter.GetPtr<FWidgetSpawnParameter>();
	UObject* OwningObject = Parameter ? Parameter->OwningObject.Get() : nullptr;
	UWorld* World = GEngine && OwningObject
		? GEngine->GetWorldFromContextObject(OwningObject, EGetWorldErrorMode::ReturnNull)
		: Module.GetWorld();
	if(!World || World->bIsTearingDown)
	{
		return nullptr;
	}
	const FObjectPoolPolicy Policy = Module.GetPoolPolicy(InClass);
	if(!Policy.bEnablePooling || Policy.MaxIdle >= 0)
	{
		return UUserWidget::CreateWidgetInstance(*World, InClass, NAME_None);
	}

	APlayerController* PlayerController = Cast<APlayerController>(OwningObject);
	UObject* Scope = OwningObject ? OwningObject : World;
	UObjectPoolWidgetBucket* Bucket = Module.FindOrAddWidgetBucket(Scope, World, PlayerController);
	UUserWidget* Widget = Bucket->Acquire(InClass);
	if(Widget)
	{
		ActiveBuckets.Add(Widget, Bucket);
	}
	return Widget;
}

bool FWidgetPoolProvider::Despawn(UObject* InObject)
{
	UUserWidget* Widget = Cast<UUserWidget>(InObject);
	if(!Widget)
	{
		return false;
	}
	Widget->RemoveFromParent();
	TWeakObjectPtr<UObjectPoolWidgetBucket>* Bucket = Widget ? ActiveBuckets.Find(Widget) : nullptr;
	if(!Bucket || !Bucket->IsValid())
	{
		return false;
	}

	const bool bReleased = Bucket->Get()->Release(Widget);
	if(!bReleased)
	{
		ActiveBuckets.Remove(Widget);
	}
	return bReleased;
}

void FWidgetPoolProvider::Destroy(UObject* InObject)
{
	if(UUserWidget* Widget = Cast<UUserWidget>(InObject))
	{
		Widget->RemoveFromParent();
		if(TWeakObjectPtr<UObjectPoolWidgetBucket>* Bucket = ActiveBuckets.Find(Widget))
		{
			if(Bucket->IsValid())
			{
				UObjectPoolWidgetBucket* BucketToClear = Bucket->Get();
				BucketToClear->Clear();
				for(auto It = ActiveBuckets.CreateIterator(); It; ++It)
				{
					if(It.Value() == BucketToClear)
					{
						It.RemoveCurrent();
					}
				}
			}
			else
			{
				ActiveBuckets.Remove(Widget);
			}
		}
	}
}

void FWidgetPoolProvider::ClearAll()
{
	ActiveBuckets.Reset();
}
