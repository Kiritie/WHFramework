// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "CharacterModuleTypes.h"
#include "Base/CharacterInterface.h"
#include "Main/Base/ModuleBase.h"

#include "CharacterModule.generated.h"

class ACharacterBase;
UCLASS()
class WHFRAMEWORK_API ACharacterModule : public AModuleBase
{
	GENERATED_BODY()
	
	GENERATED_MODULE(ACharacterModule)

public:
	// ParamSets default values for this actor's properties
	ACharacterModule();

	~ACharacterModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation(EPhase InPhase) override;

	//////////////////////////////////////////////////////////////////////////
	/// Character
protected:
	UPROPERTY(EditAnywhere, Replicated, Category = "Character")
	TArray<ACharacterBase*> Characters;

	UPROPERTY(EditAnywhere, Replicated, Category = "Character")
	ACharacterBase* DefaultCharacter;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "DefaultCharacter != nullptr"), Category = "Character")
	bool DefaultResetCamera;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "DefaultCharacter != nullptr"), Category = "Character")
	bool DefaultInstantSwitch;

	UPROPERTY(VisibleAnywhere, Replicated, Category = "Character")
	ACharacterBase* CurrentCharacter;

private:
	UPROPERTY(Transient)
	TMap<FName, ACharacterBase*> CharacterMap;

public:
	template<class T>
	T* GetCurrentCharacter() const
	{
		return Cast<T>(CurrentCharacter);
	}

	ACharacterBase* GetCurrentCharacter() const;

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InCharacterClass"))
	ACharacterBase* GetCurrentCharacter(TSubclassOf<ACharacterBase> InCharacterClass) const;

	UFUNCTION(BlueprintCallable)
	void SwitchCharacter(ACharacterBase* InCharacter, bool bResetCamera = true, bool bInstant = false);

	template<class T>
	void SwitchCharacterByClass(bool bResetCamera = true, bool bInstant = false, TSubclassOf<ACharacterBase> InCharacterClass = T::StaticClass())
	{
		SwitchCharacterByClass(InCharacterClass, bResetCamera);
	}

	UFUNCTION(BlueprintCallable)
	void SwitchCharacterByClass(TSubclassOf<ACharacterBase> InCharacterClass, bool bResetCamera = true, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	void SwitchCharacterByName(FName InCharacterName, bool bResetCamera = true, bool bInstant = false);

	template<class T>
	bool HasCharacterByClass(TSubclassOf<ACharacterBase> InCharacterClass = T::StaticClass())
	{
		return HasCharacterByClass(InCharacterClass);
	}

	UFUNCTION(BlueprintPure)
	bool HasCharacterByClass(TSubclassOf<ACharacterBase> InCharacterClass) const;

	UFUNCTION(BlueprintPure)
	bool HasCharacterByName(FName InCharacterName) const;

	template<class T>
	T* GetCharacterByClass(TSubclassOf<ACharacterBase> InCharacterClass = T::StaticClass())
	{
		return GetCharacterByClass(InCharacterClass);
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InCharacterClass"))
	ACharacterBase* GetCharacterByClass(TSubclassOf<ACharacterBase> InCharacterClass) const;

	UFUNCTION(BlueprintPure)
	ACharacterBase* GetCharacterByName(FName InCharacterName) const;

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
