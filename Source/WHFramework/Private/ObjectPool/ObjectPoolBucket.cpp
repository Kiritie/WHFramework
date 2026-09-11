#include "ObjectPool/ObjectPoolBucket.h"

void UObjectPoolBucket::Initialize(UClass* InClass, UObject* InScope, int32 InMaxIdle)
{
	ObjectClass = InClass;
	Scope = InScope;
	MaxIdle = InMaxIdle;
}

UObject* UObjectPoolBucket::Acquire()
{
	while(!FreeObjects.IsEmpty())
	{
		UObject* Object = FreeObjects.Pop(EAllowShrinking::No);
		IdleObjects.Remove(Object);
		if(IsValid(Object))
		{
			return Object;
		}
	}

	return nullptr;
}

bool UObjectPoolBucket::Release(UObject* InObject)
{
	if(!IsValid(InObject) || IdleObjects.Contains(InObject))
	{
		return false;
	}

	if(MaxIdle >= 0 && FreeObjects.Num() >= MaxIdle)
	{
		return false;
	}

	FreeObjects.Add(InObject);
	IdleObjects.Add(InObject);
	return true;
}

bool UObjectPoolBucket::Remove(UObject* InObject)
{
	IdleObjects.Remove(InObject);
	return FreeObjects.RemoveSingleSwap(InObject, EAllowShrinking::No) > 0;
}

void UObjectPoolBucket::Clear()
{
	FreeObjects.Reset();
	IdleObjects.Reset();
}

bool UObjectPoolBucket::Matches(UClass* InClass, UObject* InScope) const
{
	return ObjectClass == InClass && Scope == InScope;
}

bool UObjectPoolBucket::Contains(UObject* InObject) const
{
	return IdleObjects.Contains(InObject);
}

int32 UObjectPoolBucket::Num() const
{
	return FreeObjects.Num();
}

void UObjectPoolWidgetBucket::Initialize(UObject* InScope, UWorld* InWorld, APlayerController* InPlayerController)
{
	Scope = InScope;
	WidgetPool.SetWorld(InWorld);
	WidgetPool.SetDefaultPlayerController(InPlayerController);
}

UUserWidget* UObjectPoolWidgetBucket::Acquire(TSubclassOf<UUserWidget> InClass)
{
	UUserWidget* Widget = WidgetPool.GetOrCreateInstance(InClass);
	Widgets.Add(Widget);
	IdleWidgets.Remove(Widget);
	return Widget;
}

bool UObjectPoolWidgetBucket::Release(UUserWidget* InWidget)
{
	if(!InWidget || IdleWidgets.Contains(InWidget) || !WidgetPool.GetActiveWidgets().Contains(InWidget))
	{
		return false;
	}

	WidgetPool.Release(InWidget);
	IdleWidgets.Add(InWidget);
	return true;
}

void UObjectPoolWidgetBucket::Clear()
{
	WidgetPool.ReleaseAllSlateResources();
	WidgetPool.ResetPool();
	Widgets.Reset();
	IdleWidgets.Reset();
}

bool UObjectPoolWidgetBucket::Matches(UObject* InScope) const
{
	return Scope == InScope;
}

bool UObjectPoolWidgetBucket::Contains(UUserWidget* InWidget) const
{
	return Widgets.Contains(InWidget);
}

int32 UObjectPoolWidgetBucket::Num() const
{
	return IdleWidgets.Num();
}
