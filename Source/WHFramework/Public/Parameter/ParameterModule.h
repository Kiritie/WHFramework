// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "ParameterManager.h"
#include "ParameterModuleTypes.h"
#include "Main/Base/ModuleBase.h"
#include "SaveGame/Base/SaveDataInterface.h"
#include "ParameterModule.generated.h"

class ATargetPoint;

UCLASS()
class WHFRAMEWORK_API AParameterModule : public AModuleBase, public IParameterManager, public ISaveDataInterface
{
	GENERATED_BODY()
				
	GENERATED_MODULE(AParameterModule)

public:	
	// ParamSets default values for this actor's properties
	AParameterModule();

	~AParameterModule();

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

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData(bool bRefresh) override;

	//////////////////////////////////////////////////////////////////////////
	/// Parameter
protected:
	UPROPERTY(EditAnywhere, Replicated)
	FParameters Parameters;

public:
	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual bool HasParameter(FName InName, bool bEnsured = true) const override;

	UFUNCTION(BlueprintCallable)
	virtual void AddParameter(FName InName, FParameter InParameter) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetParameter(FName InName, FParameter InParameter) override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual FParameter GetParameter(FName InName, bool bEnsured = true) const override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual TArray<FParameter> GetParameters(FName InName, bool bEnsured = true) const override;
	
	UFUNCTION(BlueprintCallable)
	virtual void RemoveParameter(FName InName) override;
	
	UFUNCTION(BlueprintCallable)
	virtual void RemoveParameters(FName InName) override;

	UFUNCTION(BlueprintCallable)
	virtual void ClearAllParameter() override;

	//////////////////////////////////////////////////////////////////////////
	/// Integer
	UFUNCTION(BlueprintCallable)
	virtual void AddIntegerParameter(FName InName, int32 InValue) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetIntegerParameter(FName InName, int32 InValue) override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual int32 GetIntegerParameter(FName InName, bool bEnsured = true) const override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual TArray<int32> GetIntegerParameters(FName InName, bool bEnsured = true) const override;

	//////////////////////////////////////////////////////////////////////////
	/// Float
	UFUNCTION(BlueprintCallable)
	virtual void AddFloatParameter(FName InName, float InValue) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetFloatParameter(FName InName, float InValue) override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual float GetFloatParameter(FName InName, bool bEnsured = true) const override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual TArray<float> GetFloatParameters(FName InName, bool bEnsured = true) const override;

	//////////////////////////////////////////////////////////////////////////
	/// String
	UFUNCTION(BlueprintCallable)
	virtual void AddStringParameter(FName InName, const FString& InValue) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetStringParameter(FName InName, const FString& InValue) override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual FString GetStringParameter(FName InName, bool bEnsured = true) const override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual TArray<FString> GetStringParameters(FName InName, bool bEnsured = true) const override;

	//////////////////////////////////////////////////////////////////////////
	/// Text
	UFUNCTION(BlueprintCallable)
	virtual void AddTextParameter(FName InName, const FText InValue) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetTextParameter(FName InName, const FText InValue) override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual FText GetTextParameter(FName InName, bool bEnsured = true) const override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual TArray<FText> GetTextParameters(FName InName, bool bEnsured = true) const override;

	//////////////////////////////////////////////////////////////////////////
	/// Boolean
	UFUNCTION(BlueprintCallable)
	virtual void AddBooleanParameter(FName InName, bool InValue) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetBooleanParameter(FName InName, bool InValue) override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual bool GetBooleanParameter(FName InName, bool bEnsured = true) const override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual TArray<bool> GetBooleanParameters(FName InName, bool bEnsured = true) const override;

	//////////////////////////////////////////////////////////////////////////
	/// Vector
	UFUNCTION(BlueprintCallable)
	virtual void AddVectorParameter(FName InName, FVector InValue) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetVectorParameter(FName InName, FVector InValue) override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual FVector GetVectorParameter(FName InName, bool bEnsured = true) const override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual TArray<FVector> GetVectorParameters(FName InName, bool bEnsured = true) const override;

	//////////////////////////////////////////////////////////////////////////
	/// Rotator
	UFUNCTION(BlueprintCallable)
	virtual void AddRotatorParameter(FName InName, FRotator InValue) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetRotatorParameter(FName InName, FRotator InValue) override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual FRotator GetRotatorParameter(FName InName, bool bEnsured = true) const override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual TArray<FRotator> GetRotatorParameters(FName InName, bool bEnsured = true) const override;

	//////////////////////////////////////////////////////////////////////////
	/// Color
	UFUNCTION(BlueprintCallable)
	virtual void AddColorParameter(FName InName, const FColor& InValue) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetColorParameter(FName InName, const FColor& InValue) override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual FColor GetColorParameter(FName InName, bool bEnsured = true) const override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual TArray<FColor> GetColorParameters(FName InName, bool bEnsured = true) const override;

	//////////////////////////////////////////////////////////////////////////
	/// Class
	UFUNCTION(BlueprintCallable)
	virtual void AddClassParameter(FName InName, UClass* InValue) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetClassParameter(FName InName, UClass* InValue) override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual UClass* GetClassParameter(FName InName, bool bEnsured = true) const override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual TArray<UClass*> GetClassParameters(FName InName, bool bEnsured = true) const override;

	//////////////////////////////////////////////////////////////////////////
	/// Object
	UFUNCTION(BlueprintCallable)
	virtual void AddObjectParameter(FName InName, UObject* InValue) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetObjectParameter(FName InName, UObject* InValue) override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual UObject* GetObjectParameter(FName InName, bool bEnsured = true) const override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual TArray<UObject*> GetObjectParameters(FName InName, bool bEnsured = true) const override;

	//////////////////////////////////////////////////////////////////////////
	/// Pointer
	virtual void AddPointerParameter(FName InName, void* InValue) override;

	virtual void SetPointerParameter(FName InName, void* InValue) override;

	virtual void* GetPointerParameter(FName InName, bool bEnsured = true) const override;

	virtual TArray<void*> GetPointerParameters(FName InName, bool bEnsured = true) const override;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
