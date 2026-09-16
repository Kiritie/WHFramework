#pragma once
#include "Common/Base/WHObject.h"
#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Scene/Container/SceneContainerInterface.h"
#include "Voxel/Geometry/VoxelCollisionBuilder.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "VoxelChunk.generated.h"
class UVoxelModule;
class UVoxelMeshComponent;
class UVoxelCollisionComponent;
class UVoxelMaterialSet;
UCLASS()
class WHFRAMEWORK_API UVoxelChunk : public UWHObject, public ISceneContainerInterface
{
	GENERATED_BODY()
public:
	virtual UWorld* GetWorld() const override;
	bool Initialize(UVoxelModule* OwnerModule, FIntPoint Column);
	void Shutdown();
	bool ApplyMesh(const FVoxelSectionMeshResult& Result, const UVoxelMaterialSet& Materials, double BlockSize);
	bool ApplyCollision(const FVoxelSectionCollisionResult& Result, double BlockSize);
	void ClearMesh(int32 SectionZ);
	void ClearCollision(int32 SectionZ);
	FIntPoint GetColumn() const
	{
		return ColumnIndex;
	}
	bool CaptureActors(TArray<uint8>& Out, FString& Error) const;
	bool RestoreActors(TConstArrayView<uint8> Bytes, FString& Error);
	virtual bool ShouldPersistActor(AActor* Actor) const;
	virtual void OnSectionActivated(const FVoxelSectionKey& Key);
	virtual void TickSceneActors(float DeltaSeconds);
	virtual FParameter CaptureProjectData() const;
	virtual bool RestoreProjectData(const FParameter& Data, FString& Error);
	virtual bool HasSceneActor(const FString& ID, bool bEnsured = true) const override;
	virtual AActor* GetSceneActor(const FString& ID, TSubclassOf<AActor> Class = nullptr, bool bEnsured = true) const override;
	virtual bool AddSceneActor(AActor* Actor) override;
	virtual bool RemoveSceneActor(AActor* Actor) override;
	virtual void DestroySceneActors() override;
	const TMap<FGuid, TObjectPtr<AActor>>& GetSceneActors() const
	{
		return SceneActorMap;
	}
	bool bSceneReady = false;
	bool bSceneLoading = false;
	bool bSceneFailed = false;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UVoxelModule> Module;
	UPROPERTY(Transient)
	TObjectPtr<AActor> Host;
	UPROPERTY(Transient)
	TMap<uint64, TObjectPtr<UVoxelMeshComponent>> Meshes;
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UVoxelCollisionComponent>> Collisions;
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<AActor>> SceneActorMap;
	FIntPoint ColumnIndex = FIntPoint::ZeroValue;

private:
	static uint64 MeshKey(int32 Z, EVoxelRenderGroup Group, uint16 Bank);
};
