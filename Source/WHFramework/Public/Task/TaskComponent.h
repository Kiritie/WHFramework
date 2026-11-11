#pragma once

#include "Task/TaskExecutionTypes.h"
#include "Task/TaskReferenceTypes.h"
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
	virtual TArray<UTaskAsset*> GetOfferedAssets(AActor* InInteractor) const;

	UFUNCTION(BlueprintPure)
	virtual UTaskBase* GetTask(AActor* InInteractor, ETaskStage InStage) const;

	UFUNCTION(BlueprintPure)
	virtual UTaskBase* ResolveTask(AActor* InInteractor, const FTaskReference& InReference) const;

	UFUNCTION(BlueprintCallable)
	UTaskBase* AcceptTask(UTaskAsset* InAsset = nullptr, FString InTaskGUID = FString(), AActor* InInteractor = nullptr);

	UFUNCTION(BlueprintCallable)
	bool TurnInTask(AActor* InInteractor, UTaskBase* InTask = nullptr);

	UFUNCTION(BlueprintCallable)
	void SetAgentActive(bool bInActive);

	UFUNCTION(BlueprintPure)
	bool HasAvailableTask(AActor* InInteractor) const;

	UFUNCTION(BlueprintPure)
	FGuid GetAgentID() const { return AgentID; }

	UFUNCTION(BlueprintCallable)
	void RefreshTaskMarker();

	UPROPERTY(BlueprintAssignable)
	FOnTaskAccepted OnTaskAccepted;

	UPROPERTY(BlueprintAssignable)
	FOnTaskAvailabilityChanged OnTaskAvailabilityChanged;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<TSoftObjectPtr<UTaskAsset>> TaskAssets;

	UPROPERTY(Transient)
	TArray<UTaskAsset*> ResolvedTaskAssets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> DeliverableMarkerIcon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector MarkerOffset = FVector(0.f, 0.f, 120.f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FGuid AgentID;

	FGuid MarkerID;
	FGameplayTag LastMarkerTag;
	bool bAgentActive = true;
	bool bHasAvailableTask = false;

	UFUNCTION()
	void OnTaskAssetsChanged();

	bool ResolveAgentID();
	void ResolveTaskAssets();
	bool HasRuntimeAsset(UTaskAsset* InSource, FGuid InPlayerId) const;
};
