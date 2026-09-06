#pragma once

#include "Components/ActorComponent.h"
#include "Task/TaskModuleTypes.h"
#include "TaskComponent.generated.h"

class UTaskAsset;
class UTaskBase;
class UTexture2D;

UCLASS(ClassGroup = "Task", meta = (BlueprintSpawnableComponent))
class WHFRAMEWORK_API UTaskComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTaskComponent(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	UFUNCTION(BlueprintCallable)
	void SetTaskAssets(const TArray<TSoftObjectPtr<UTaskAsset>>& InAssets);

	UFUNCTION(BlueprintPure)
	TArray<TSoftObjectPtr<UTaskAsset>> GetTaskAssets() const { return TaskAssets; }

	UFUNCTION(BlueprintPure)
	TArray<UTaskAsset*> GetRuntimeAssets() const;

	UFUNCTION(BlueprintPure)
	TArray<UTaskAsset*> GetOfferedAssets() const;

	UFUNCTION(BlueprintPure)
	UTaskBase* GetTask(ETaskStage InStage) const;

	UFUNCTION(BlueprintPure)
	UTaskBase* ResolveTask(const FTaskReference& InReference) const;

	UFUNCTION(BlueprintCallable)
	UTaskBase* AcceptTask(UTaskAsset* InAsset = nullptr, FString InTaskGUID = FString());

	UFUNCTION(BlueprintCallable)
	bool TurnInTask(UTaskBase* InTask = nullptr);

	UFUNCTION(BlueprintCallable)
	void SetAgentActive(bool bInActive);

	UFUNCTION(BlueprintPure)
	FGuid GetAgentID() const { return AgentID; }

	UFUNCTION(BlueprintCallable)
	void RefreshTaskMarker();

	UPROPERTY(BlueprintAssignable)
	FOnTaskAccepted OnTaskAccepted;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<TSoftObjectPtr<UTaskAsset>> TaskAssets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> AvailableMarkerIcon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> DeliverableMarkerIcon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector MarkerOffset = FVector(0.f, 0.f, 120.f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FGuid AgentID;

	FGuid MarkerID;
	FGameplayTag LastMarkerTag;
	bool bAgentActive = true;

	UFUNCTION()
	void OnTaskAssetsChanged();

	bool ResolveAgentID();
	bool HasRuntimeAsset(UTaskAsset* InSource) const;
};
