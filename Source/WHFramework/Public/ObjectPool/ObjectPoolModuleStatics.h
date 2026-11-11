#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"
#include "ObjectPool/ObjectPoolModule.h"
#include "ObjectPoolModuleStatics.generated.h"

UCLASS()
class WHFRAMEWORK_API UObjectPoolModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"), Category = "ObjectPoolModule")
	static UObject* SpawnObject(const UObject* InWorldContext, UClass* InClass, const FParameter& InParam);

	template<class TObject>
	static TObject* SpawnObject(const UObject* InWorldContext, TSubclassOf<TObject> InClass = TObject::StaticClass())
	{
		UObjectPoolModule* Pool = InWorldContext ? UObjectPoolModule::GetPtr(InWorldContext->GetWorld()) : nullptr;
		return Pool ? Pool->SpawnObject<TObject>(InClass) : nullptr;
	}

	template<class TObject, class TSpawnParameter>
		requires std::is_base_of_v<FSpawnParameter, std::decay_t<TSpawnParameter>>
	static TObject* SpawnObject(
		const UObject* InWorldContext, const TSpawnParameter& InParam,
		TSubclassOf<TObject> InClass = TObject::StaticClass())
	{
		UObjectPoolModule* Pool = InWorldContext ? UObjectPoolModule::GetPtr(InWorldContext->GetWorld()) : nullptr;
		return Pool ? Pool->SpawnObject<TObject>(InParam, InClass) : nullptr;
	}

	UFUNCTION(BlueprintCallable, Category = "ObjectPoolModule")
	static void DespawnObject(
		const UObject* InWorldContext, UObject* InObject,
		EObjectDespawnMode InMode = EObjectDespawnMode::Recovery);

	template<class TObject>
	static void DespawnObjects(const UObject* InWorldContext, const TArray<TObject*>& InObjects,
		EObjectDespawnMode InMode = EObjectDespawnMode::Recovery)
	{
		for(TObject* Object : InObjects)
		{
			DespawnObject(InWorldContext, Object, InMode);
		}
	}

	UFUNCTION(BlueprintCallable, Category = "ObjectPoolModule")
	static void DespawnObjects(const UObject* InWorldContext, const TArray<UObject*>& InObjects,
		EObjectDespawnMode InMode = EObjectDespawnMode::Recovery)
	{
		for(UObject* Object : InObjects)
		{
			DespawnObject(InWorldContext, Object, InMode);
		}
	}

	UFUNCTION(BlueprintCallable, Category = "ObjectPoolModule")
	static void ClearObject(const UObject* InWorldContext, TSubclassOf<UObject> InClass);

	UFUNCTION(BlueprintCallable, Category = "ObjectPoolModule")
	static void ClearAllObject(const UObject* InWorldContext);
};
