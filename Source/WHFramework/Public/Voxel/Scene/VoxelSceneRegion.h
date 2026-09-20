#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Common/Base/WHObject.h"
#include "Scene/Container/SceneContainerInterface.h"
#include "VoxelSceneRegion.generated.h"

class UVoxelModule;

UCLASS(Abstract)
class WHFRAMEWORK_API UVoxelSceneRegion : public UWHObject, public ISceneContainerInterface
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;
	virtual void Initialize(UVoxelModule* InModule, const FIntVector& InRegionKey);
	virtual void Shutdown();
	virtual void OnSectionActivated(const FIntVector& InSection);
	virtual void OnSectionDeactivated(const FIntVector& InSection);
	virtual void TickSceneActors(float InDeltaSeconds);
	virtual bool CaptureProjectData(FParameter& OutData) const;
	virtual bool RestoreProjectData(const FParameter& InData, FString& OutError);
	virtual bool ShouldPersistActor(AActor* InActor) const;

	bool CaptureActors(TArray<uint8>& OutBytes, FString& OutError) const;
	bool RestoreActors(TConstArrayView<uint8> InBytes, FString& OutError);

	virtual bool HasSceneActor(const FString& InId, bool bInEnsured = true) const override;
	virtual AActor* GetSceneActor(
		const FString& InId,
		TSubclassOf<AActor> InClass = nullptr,
		bool bInEnsured = true) const override;
	virtual bool AddSceneActor(AActor* InActor) override;
	virtual bool RemoveSceneActor(AActor* InActor) override;
	virtual void DestroySceneActors() override;

	const FIntVector& GetRegionKey() const;
	const TMap<FGuid, TObjectPtr<AActor>>& GetSceneActors() const;
	bool HasActiveSections() const;

	bool bSceneReady = false;
	bool bSceneLoading = false;
	bool bSceneFailed = false;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UVoxelModule> Module = nullptr;

	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<AActor>> SceneActorMap;

	FIntVector RegionKey = FIntVector::ZeroValue;
	TSet<FIntVector> ActiveSections;
};
