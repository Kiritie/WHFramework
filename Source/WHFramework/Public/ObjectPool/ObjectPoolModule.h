#pragma once

#include "Main/Base/ModuleBase.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"
#include "ObjectPool/Provider/ActorPoolProvider.h"
#include "ObjectPool/Provider/ObjectPoolProvider.h"
#include "ObjectPool/Provider/WidgetPoolProvider.h"
#include "Parameter/ParameterModuleTypes.h"
#include "ObjectPoolModule.generated.h"

class IObjectPoolProvider;
class UObjectPoolBucket;
class UObjectPoolWidgetBucket;

UCLASS()
class WHFRAMEWORK_API UObjectPoolModule : public UModuleBase
{
	GENERATED_BODY()

	GENERATED_MODULE(UObjectPoolModule)

public:
	UObjectPoolModule();

	virtual ~UObjectPoolModule();

#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

	virtual FString GetModuleDebugMessage() override;

public:
	UFUNCTION(BlueprintCallable)
	UObject* SpawnObject(UClass* InClass, const FParameter& InParameter = FParameter());

	template<class TObject>
	TObject* SpawnObject(TSubclassOf<TObject> InClass = TObject::StaticClass())
	{
		return Cast<TObject>(SpawnObject(InClass.Get(), FParameter()));
	}

	template<class TObject, class TSpawnParameter>
		requires std::is_base_of_v<FSpawnParameter, std::decay_t<TSpawnParameter>>
	TObject* SpawnObject(
		const TSpawnParameter& InParameter,
		TSubclassOf<TObject> InClass = TObject::StaticClass())
	{
		return Cast<TObject>(SpawnObject(InClass.Get(), FParameter(InParameter)));
	}

	UFUNCTION(BlueprintCallable)
	void DespawnObject(
		UObject* InObject,
		EObjectDespawnMode InMode = EObjectDespawnMode::Recovery);

	template<class TObject>
	void DespawnObjects(const TArray<TObject*>& InObjects)
	{
		for(TObject* Object : InObjects)
		{
			DespawnObject(Object);
		}
	}

	UFUNCTION(BlueprintCallable)
	void DespawnObjects(const TArray<UObject*>& InObjects);

	UFUNCTION(BlueprintCallable)
	void ClearObject(TSubclassOf<UObject> InClass);

	UFUNCTION(BlueprintCallable)
	void ClearAllObject();

	UFUNCTION(BlueprintPure)
	FObjectPoolPolicy GetPoolPolicy(TSubclassOf<UObject> InClass) const;

	UFUNCTION(BlueprintCallable)
	void SetPoolPolicy(TSubclassOf<UObject> InClass, const FObjectPoolPolicy& InPolicy);

public:
	void DestroyObject(UObject* InObject);

	UObjectPoolBucket* FindOrAddGenericBucket(UClass* InClass, UObject* InScope, int32 InMaxIdle);

	UObjectPoolWidgetBucket* FindOrAddWidgetBucket(
		UObject* InScope,
		UWorld* InWorld,
		APlayerController* InPlayerController);

	void RemoveFromGenericBucket(UObject* InObject);

private:
	IObjectPoolProvider* ResolveProvider(UClass* InClass) const;

	bool IsInactive(UObject* InObject) const;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObjectPoolBucket>> GenericBuckets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObjectPoolWidgetBucket>> WidgetBuckets;

	UPROPERTY(Transient)
	TMap<TSubclassOf<UObject>, FObjectPoolPolicy> PoolPolicies;

	TSet<TWeakObjectPtr<UObject>> InactiveObjects;

	TUniquePtr<FObjectPoolProvider> ObjectProvider;

	TUniquePtr<FActorPoolProvider> ActorProvider;

	TUniquePtr<FWidgetPoolProvider> WidgetProvider;
};
