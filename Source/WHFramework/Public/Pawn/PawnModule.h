// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Common/LifecycleTypes.h"
#include "Main/MainModuleAccess.h"
#include "PawnModuleTypes.h"
#include "Main/Base/ModuleBase.h"

#include "PawnModule.generated.h"

class APawnBase;
class APlayerController;
UCLASS()
class WHFRAMEWORK_API UPawnModule : public UModuleBase
{
	GENERATED_BODY()
	
	GENERATED_MODULE(UPawnModule)

public:
	// ParamSets default values for this actor's properties
	UPawnModule();

	~UPawnModule();

	//////////////////////////////////////////////////////////////////////////
	/// ModuleBase
public:
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

public:
	virtual FString GetModuleDebugMessage() override;

	//////////////////////////////////////////////////////////////////////////
	/// Pawn
protected:
	UPROPERTY(EditAnywhere, Replicated, Category = "Pawn")
	TArray<APawnBase*> Pawns;

	UPROPERTY(EditAnywhere, Replicated, Category = "Pawn")
	APawnBase* DefaultPawn;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "DefaultPawn != nullptr"), Category = "Pawn")
	bool DefaultResetCamera;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "DefaultPawn != nullptr"), Category = "Pawn")
	bool DefaultInstantSwitch;

private:
	UPROPERTY(Transient)
	TMap<FName, APawnBase*> PawnMap;
	TSet<TWeakObjectPtr<APlayerController>> SwitchingPlayers;

public:
	template<class T>
	T* GetCurrentPawn(APlayerController* InPlayer) const
	{
		return Cast<T>(GetCurrentPawn(InPlayer, T::StaticClass()));
	}

	UFUNCTION(BlueprintPure)
	TArray<APawnBase*> GetAllPawn() const { return Pawns; }

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	APawnBase* GetCurrentPawn(APlayerController* InPlayer, TSubclassOf<APawnBase> InClass) const;

	UFUNCTION(BlueprintCallable)
	bool SwitchPawn(APlayerController* InPlayer, APawnBase* InPawn, bool bResetCamera = true, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	bool SwitchToDefaultPawn(APlayerController* InPlayer);

	template<class T>
	bool SwitchPawnByClass(APlayerController* InPlayer, bool bResetCamera = true, bool bInstant = false, TSubclassOf<APawnBase> InClass = T::StaticClass())
	{
		return SwitchPawnByClass(InPlayer, InClass, bResetCamera, bInstant);
	}

	UFUNCTION(BlueprintCallable)
	bool SwitchPawnByClass(APlayerController* InPlayer, TSubclassOf<APawnBase> InClass, bool bResetCamera = true, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	bool SwitchPawnByName(APlayerController* InPlayer, FName InName, bool bResetCamera = true, bool bInstant = false);

	template<class T>
	bool HasPawnByClass(TSubclassOf<APawnBase> InClass = T::StaticClass())
	{
		return HasPawnByClass(InClass);
	}

	UFUNCTION(BlueprintPure)
	bool HasPawnByClass(TSubclassOf<APawnBase> InClass) const;

	UFUNCTION(BlueprintPure)
	bool HasPawnByName(FName InName) const;

	template<class T>
	T* GetPawnByClass(TSubclassOf<APawnBase> InClass = T::StaticClass())
	{
		return GetPawnByClass(InClass);
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	APawnBase* GetPawnByClass(TSubclassOf<APawnBase> InClass) const;

	UFUNCTION(BlueprintPure)
	APawnBase* GetPawnByName(FName InName) const;

public:
	UFUNCTION(BlueprintCallable)
	void AddPawnToList(APawnBase* InPawn);

	UFUNCTION(BlueprintCallable)
	void RemovePawnFromList(APawnBase* InPawn);

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
