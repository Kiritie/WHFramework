// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "ParameterModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ParameterModuleBPLibrary.generated.h"

class AParameterModule;
class ATargetPoint;

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UParameterModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static AParameterModule* ParameterModuleInst;

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static AParameterModule* GetParameterModule(UObject* InWorldContext);

	//////////////////////////////////////////////////////////////////////////
	/// Parameter
	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static bool HasParameter(UObject* InWorldContext, FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static void SetParameter(UObject* InWorldContext, FName InName, FParameter InParameter);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static FParameter GetParameter(UObject* InWorldContext, FName InName, bool bEnsured);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static TArray<FParameter> GetParameters(UObject* InWorldContext, FName InName, bool bEnsured);

	//////////////////////////////////////////////////////////////////////////
	/// Integer
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static void SetIntegerParameter(UObject* InWorldContext, FName InName, int32 InValue);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static int32 GetIntegerParameter(UObject* InWorldContext, FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static TArray<int32> GetIntegerParameters(UObject* InWorldContext, FName InName, bool bEnsured = true);
	
	//////////////////////////////////////////////////////////////////////////
	/// Float
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static void SetFloatParameter(UObject* InWorldContext, FName InName, float InValue);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static float GetFloatParameter(UObject* InWorldContext, FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static TArray<float> GetFloatParameters(UObject* InWorldContext, FName InName, bool bEnsured = true);
	
	//////////////////////////////////////////////////////////////////////////
	/// String
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static void SetStringParameter(UObject* InWorldContext, FName InName, const FString& InValue);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static FString GetStringParameter(UObject* InWorldContext, FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static TArray<FString> GetStringParameters(UObject* InWorldContext, FName InName, bool bEnsured = true);
	
	//////////////////////////////////////////////////////////////////////////
	/// Boolean
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static void SetBooleanParameter(UObject* InWorldContext, FName InName, bool InValue);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static bool GetBooleanParameter(UObject* InWorldContext, FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static TArray<bool> GetBooleanParameters(UObject* InWorldContext, FName InName, bool bEnsured = true);
	
	//////////////////////////////////////////////////////////////////////////
	/// Vector
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static void SetVectorParameter(UObject* InWorldContext, FName InName, FVector InValue);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static FVector GetVectorParameter(UObject* InWorldContext, FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static TArray<FVector> GetVectorParameters(UObject* InWorldContext, FName InName, bool bEnsured = true);
	
	//////////////////////////////////////////////////////////////////////////
	/// Rotator
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static void SetRotatorParameter(UObject* InWorldContext, FName InName, FRotator InValue);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static FRotator GetRotatorParameter(UObject* InWorldContext, FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static TArray<FRotator> GetRotatorParameters(UObject* InWorldContext, FName InName, bool bEnsured = true);
	
	//////////////////////////////////////////////////////////////////////////
	/// Class
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static void SetClassParameter(UObject* InWorldContext, FName InName, UClass* InValue);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static UClass* GetClassParameter(UObject* InWorldContext, FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static TArray<UClass*> GetClassParameters(UObject* InWorldContext, FName InName, bool bEnsured = true);
	
	//////////////////////////////////////////////////////////////////////////
	/// Object
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static void SetObjectParameter(UObject* InWorldContext, FName InName, UObject* InValue);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static UObject* GetObjectParameter(UObject* InWorldContext, FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "ParameterModuleBPLibrary")
	static TArray<UObject*> GetObjectParameters(UObject* InWorldContext, FName InName, bool bEnsured = true);

	//////////////////////////////////////////////////////////////////////////
	/// MakeParameter
public:
	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FParameter MakeIntegerParameter(int32 InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FParameter MakeFloatParameter(float InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FParameter MakeStringParameter(FString InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FParameter MakeBooleanParameter(bool InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FParameter MakeVectorParameter(FVector InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FParameter MakeRotatorParameter(FRotator InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FParameter MakeClassParameter(UClass* InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FParameter MakeObjectParameter(UObject* InValue);

	//////////////////////////////////////////////////////////////////////////
	/// GetParameter
public:
	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static int32 GetIntegerValue(const FParameter& InParameter);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static float GetFloatValue(const FParameter& InParameter);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FString GetStringValue(const FParameter& InParameter);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static bool GetBooleanValue(const FParameter& InParameter);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FVector GetVectorValue(const FParameter& InParameter);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FRotator GetRotatorValue(const FParameter& InParameter);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static UClass* GetClassValue(const FParameter& InParameter);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static UObject* GetObjectValue(const FParameter& InParameter);
};
