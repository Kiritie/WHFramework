// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/CommonModuleTypes.h"
#include "Components/BoxComponent.h"
#include "InteractionComponent.generated.h"

class IInteractionAgentInterface;
class UInteractionOptionBase;
class UInteractionActionExecution;
/**
 * 
 */
UCLASS(ClassGroup="Collision", editinlinenew, meta=(DisplayName="Interaction", BlueprintSpawnableComponent))
class WHFRAMEWORK_API UInteractionComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UInteractionComponent(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	virtual void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	virtual void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

public:
	virtual bool OnAgentEnter(IInteractionAgentInterface* InInteractionAgent, bool bPassive = false);

	virtual bool OnAgentLeave(IInteractionAgentInterface* InInteractionAgent, bool bPassive = false);

public:
	UFUNCTION(BlueprintPure)
	TArray<AActor*> GetAvailableTargets() const;

	UFUNCTION(BlueprintCallable)
	bool NextTarget();

	UFUNCTION(BlueprintPure)
	AActor* GetSelectedTarget() const { return SelectedTarget.Get(); }

	UFUNCTION(BlueprintCallable)
	void SetSelectedTarget(AActor* InTarget);

	UPROPERTY(EditAnywhere)
	bool bAutoSelectTarget = false;

	bool BeginInteraction(AActor* InInteractor, UObject* InOwner);
	bool CanBeginInteraction(AActor* InInteractor) const;
	void EndInteraction(UObject* InOwner);
	void RefreshTargets();
	void OnActionEnded(UInteractionActionExecution* InExecution);
	void CancelInteractions();
	void FinishActions(FGameplayTag InOptionTag);
	UInteractionActionExecution* GetRunningAction(FGameplayTag InOptionTag) const;

	UPROPERTY(BlueprintAssignable)
	FOnInteractionTargetsChanged OnTargetsChanged;

	UPROPERTY(BlueprintAssignable)
	FOnInteractionCancelled OnInteractionCancelled;

	UFUNCTION(BlueprintPure)
	TArray<FInteractionOptionView> GetOptions(AActor* InInteractor) const;

	UFUNCTION(BlueprintCallable)
	bool ExecuteOption(AActor* InInteractor, FGameplayTag InOptionTag, FText& OutReason);

	UFUNCTION(BlueprintCallable)
	void NotifyOptionsChanged();

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly)
	TArray<UInteractionOptionBase*> Options;

	UPROPERTY(BlueprintAssignable)
	FOnInteractionOptionsChanged OnOptionsChanged;

protected:
	TSet<FGameplayTag> ExecutingOptions;
	TSet<TWeakObjectPtr<UObject>> InteractionOwners;
	UPROPERTY(Transient)
	TArray<UInteractionActionExecution*> ActiveActions;
	bool bEndingPlay = false;
	bool bCancelling = false;
	TWeakObjectPtr<AActor> SelectedTarget;
	void NotifyAvailabilityChanged();
	FInteractionContext MakeInteractionContext(AActor* InInteractor) const;

public:
	virtual bool IsInteractable() const;
	
	virtual void SetInteractable(bool bValue);

	virtual IInteractionAgentInterface* GetInteractionAgent() const;
};
