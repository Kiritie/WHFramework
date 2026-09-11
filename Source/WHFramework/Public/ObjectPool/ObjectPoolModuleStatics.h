#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ObjectPool/ObjectPoolModule.h"
#include "ObjectPoolModuleStatics.generated.h"

UCLASS()
class WHFRAMEWORK_API UObjectPoolModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"), Category = "ObjectPoolModule")
	static UObject* SpawnObject(UClass* InClass, const FParameter& InParameter);

	template<class TObject>
	static TObject* SpawnObject(TSubclassOf<TObject> InClass = TObject::StaticClass())
	{
		return UObjectPoolModule::Get().SpawnObject<TObject>(InClass);
	}

	template<class TObject, class TSpawnParameter>
		requires std::is_base_of_v<FSpawnParameter, std::decay_t<TSpawnParameter>>
	static TObject* SpawnObject(
		const TSpawnParameter& InParameter,
		TSubclassOf<TObject> InClass = TObject::StaticClass())
	{
		return UObjectPoolModule::Get().SpawnObject<TObject>(InParameter, InClass);
	}

	UFUNCTION(BlueprintCallable, Category = "ObjectPoolModule")
	static void DespawnObject(
		UObject* InObject,
		EObjectDespawnMode InMode = EObjectDespawnMode::Recovery);

	template<class TObject>
	static void DespawnObjects(const TArray<TObject*>& InObjects,
		EObjectDespawnMode InMode = EObjectDespawnMode::Recovery)
	{
		for(TObject* Object : InObjects)
		{
			DespawnObject(Object);
		}
	}

	UFUNCTION(BlueprintCallable, Category = "ObjectPoolModule")
	static void DespawnObjects(const TArray<UObject*>& InObjects,
		EObjectDespawnMode InMode = EObjectDespawnMode::Recovery)
	{
		for(UObject* Object : InObjects)
		{
			DespawnObject(Object);
		}
	}

	UFUNCTION(BlueprintCallable, Category = "ObjectPoolModule")
	static void ClearObject(TSubclassOf<UObject> InClass);

	UFUNCTION(BlueprintCallable, Category = "ObjectPoolModule")
	static void ClearAllObject();
};
