// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Common/LifecycleTypes.h"
#include "Main/MainModuleAccess.h"
#include "CharacterModuleTypes.h"
#include "Main/Base/ModuleBase.h"

#include "CharacterModule.generated.h"

class ACharacterBase;
class APlayerController;
UCLASS()
class WHFRAMEWORK_API UCharacterModule : public UModuleBase
{
	GENERATED_BODY()
	
	GENERATED_MODULE(UCharacterModule)

public:
	// ParamSets default values for this actor's properties
	UCharacterModule();

	~UCharacterModule();

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
	
protected:
	virtual void LoadData(const FParameter& InSaveData, EPhase InPhase) override;
	virtual bool WasLoadDataApplied() const override { return bCharacterLoadSucceeded; }

	virtual FParameter ToData() override;

	virtual bool ShouldSaveCharacter(const ACharacterBase* InCharacter) const;


public:
	virtual FString GetModuleDebugMessage() override;

	//////////////////////////////////////////////////////////////////////////
	/// Character
protected:
	bool bCharacterLoadSucceeded = false;
	UPROPERTY(EditAnywhere, Replicated, Category = "Character")
	TArray<ACharacterBase*> Characters;

private:
	UPROPERTY(Transient)
	TMap<FName, ACharacterBase*> CharacterMap;

public:
	template<class T>
	T* GetCurrentCharacter(APlayerController* InController) const
	{
		return Cast<T>(GetCurrentCharacter(InController));
	}

	UFUNCTION(BlueprintPure)
	TArray<ACharacterBase*> GetAllCharacter() const { return Characters; }

	ACharacterBase* GetCurrentCharacter(APlayerController* InController) const;

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	ACharacterBase* GetCurrentCharacter(APlayerController* InController, TSubclassOf<ACharacterBase> InClass) const;

	UFUNCTION(BlueprintCallable)
	void SwitchCharacter(APlayerController* InController, ACharacterBase* InCharacter, bool bResetCamera = true, bool bInstant = false);

	template<class T>
	void SwitchCharacterByClass(APlayerController* InController, bool bResetCamera = true, bool bInstant = false, TSubclassOf<ACharacterBase> InClass = T::StaticClass())
	{
		SwitchCharacterByClass(InController, InClass, bResetCamera, bInstant);
	}

	UFUNCTION(BlueprintCallable)
	void SwitchCharacterByClass(APlayerController* InController, TSubclassOf<ACharacterBase> InClass, bool bResetCamera = true, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	void SwitchCharacterByName(APlayerController* InController, FName InName, bool bResetCamera = true, bool bInstant = false);

	template<class T>
	bool HasCharacterByClass(TSubclassOf<ACharacterBase> InClass = T::StaticClass())
	{
		return HasCharacterByClass(InClass);
	}

	UFUNCTION(BlueprintPure)
	bool HasCharacterByClass(TSubclassOf<ACharacterBase> InClass) const;

	UFUNCTION(BlueprintPure)
	bool HasCharacterByName(FName InName) const;

	template<class T>
	T* GetCharacterByClass(TSubclassOf<ACharacterBase> InClass = T::StaticClass())
	{
		return Cast<T>(GetCharacterByClass(InClass));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	ACharacterBase* GetCharacterByClass(TSubclassOf<ACharacterBase> InClass) const;

	UFUNCTION(BlueprintPure)
	ACharacterBase* GetCharacterByName(FName InName) const;

public:
	UFUNCTION(BlueprintCallable)
	void AddCharacterToList(ACharacterBase* InCharacter);

	UFUNCTION(BlueprintCallable)
	void RemoveCharacterFromList(ACharacterBase* InCharacter);

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
