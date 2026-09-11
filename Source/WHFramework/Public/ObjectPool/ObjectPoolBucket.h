#pragma once

#include "Blueprint/UserWidgetPool.h"
#include "CoreMinimal.h"
#include "ObjectPoolBucket.generated.h"

UCLASS()
class WHFRAMEWORK_API UObjectPoolBucket : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UClass* InClass, UObject* InScope, int32 InMaxIdle);

	UObject* Acquire();

	bool Release(UObject* InObject);

	bool Remove(UObject* InObject);

	void Clear();

	bool Matches(UClass* InClass, UObject* InScope) const;

	bool Contains(UObject* InObject) const;

	int32 Num() const;

private:
	UPROPERTY(Transient)
	TSubclassOf<UObject> ObjectClass;

	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> Scope;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> FreeObjects;

	TSet<TWeakObjectPtr<UObject>> IdleObjects;

	int32 MaxIdle = -1;
};

UCLASS()
class WHFRAMEWORK_API UObjectPoolWidgetBucket : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UObject* InScope, UWorld* InWorld, APlayerController* InPlayerController);

	UUserWidget* Acquire(TSubclassOf<UUserWidget> InClass);

	bool Release(UUserWidget* InWidget);

	void Clear();

	bool Matches(UObject* InScope) const;

	bool Contains(UUserWidget* InWidget) const;

	int32 Num() const;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> Scope;

	UPROPERTY(Transient)
	FUserWidgetPool WidgetPool;

	TSet<TWeakObjectPtr<UUserWidget>> Widgets;

	TSet<TWeakObjectPtr<UUserWidget>> IdleWidgets;
};
