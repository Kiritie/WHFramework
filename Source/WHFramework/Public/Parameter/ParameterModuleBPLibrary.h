// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


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
	//////////////////////////////////////////////////////////////////////////
	/// Parameter
	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static bool HasGlobalParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void AddGlobalParameter(FName InName, FParameter InParameter);

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetGlobalParameter(FName InName, FParameter InParameter);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FParameter GetGlobalParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<FParameter> GetGlobalParameters(FName InName, bool bEnsured = true);
	
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void RemoveGlobalParameter(FName InName);
	
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void RemoveGlobalParameters(FName InName);

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void ClearAllGlobalParameter();

	//////////////////////////////////////////////////////////////////////////
	/// Integer
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void AddGlobalIntegerParameter(FName InName, int32 InValue);

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetGlobalIntegerParameter(FName InName, int32 InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static int32 GetGlobalIntegerParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<int32> GetGlobalIntegerParameters(FName InName, bool bEnsured = true);
	
	//////////////////////////////////////////////////////////////////////////
	/// Float
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void AddGlobalFloatParameter(FName InName, float InValue);

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetGlobalFloatParameter(FName InName, float InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static float GetGlobalFloatParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<float> GetGlobalFloatParameters(FName InName, bool bEnsured = true);
	
	//////////////////////////////////////////////////////////////////////////
	/// String
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void AddGlobalStringParameter(FName InName, const FString& InValue);

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetGlobalStringParameter(FName InName, const FString& InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FString GetGlobalStringParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<FString> GetGlobalStringParameters(FName InName, bool bEnsured = true);
		
	//////////////////////////////////////////////////////////////////////////
	/// Text
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void AddGlobalTextParameter(FName InName, const FText InValue);

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetGlobalTextParameter(FName InName, const FText InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FText GetGlobalTextParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<FText> GetGlobalTextParameters(FName InName, bool bEnsured = true);

	//////////////////////////////////////////////////////////////////////////
	/// Boolean
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void AddGlobalBooleanParameter(FName InName, bool InValue);

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetGlobalBooleanParameter(FName InName, bool InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static bool GetGlobalBooleanParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<bool> GetGlobalBooleanParameters(FName InName, bool bEnsured = true);
	
	//////////////////////////////////////////////////////////////////////////
	/// Vector
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void AddGlobalVectorParameter(FName InName, FVector InValue);

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetGlobalVectorParameter(FName InName, FVector InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FVector GetGlobalVectorParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<FVector> GetGlobalVectorParameters(FName InName, bool bEnsured = true);
	
	//////////////////////////////////////////////////////////////////////////
	/// Rotator
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void AddGlobalRotatorParameter(FName InName, FRotator InValue);

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetGlobalRotatorParameter(FName InName, FRotator InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FRotator GetGlobalRotatorParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<FRotator> GetGlobalRotatorParameters(FName InName, bool bEnsured = true);
		
	//////////////////////////////////////////////////////////////////////////
	/// Color
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void AddGlobalColorParameter(FName InName, const FColor& InValue);

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetGlobalColorParameter(FName InName, const FColor& InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FColor GetGlobalColorParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<FColor> GetGlobalColorParameters(FName InName, bool bEnsured = true);

	//////////////////////////////////////////////////////////////////////////
	/// Class
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void AddGlobalClassParameter(FName InName, UClass* InValue);

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetGlobalClassParameter(FName InName, UClass* InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static UClass* GetGlobalClassParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<UClass*> GetGlobalClassParameters(FName InName, bool bEnsured = true);
	
	//////////////////////////////////////////////////////////////////////////
	/// Object
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void AddGlobalObjectParameter(FName InName, UObject* InValue);

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetGlobalObjectParameter(FName InName, UObject* InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static UObject* GetGlobalObjectParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<UObject*> GetGlobalObjectParameters(FName InName, bool bEnsured = true);

	//////////////////////////////////////////////////////////////////////////
	/// Pointer
	static void AddPointerParameter(FName InName, void* InValue);

	static void SetPointerParameter(FName InName, void* InValue);

	static void* GetPointerParameter(FName InName, bool bEnsured = true);

	static TArray<void*> GetPointerParameters(FName InName, bool bEnsured = true);

	//////////////////////////////////////////////////////////////////////////
	/// Parameter
public:
	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FParameter MakeIntegerParameter(int32 InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FParameter MakeFloatParameter(float InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FParameter MakeStringParameter(FString InValue);
	
	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FParameter MakeTextParameter(FText InValue);

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

public:
	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static EParameterType GetParameterType(UPARAM(ref) FParameter& InParameter) { return InParameter.GetParameterType(); }

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetParameterType(UPARAM(ref) FParameter& InParameter, EParameterType InParameterType) { InParameter.SetParameterType(InParameterType); }

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static int32 GetIntegerValue(UPARAM(ref) FParameter& InParameter) { return InParameter.GetIntegerValue(); }

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetIntegerValue(UPARAM(ref) FParameter& InParameter, int32 InIntegerValue) { InParameter.SetIntegerValue(InIntegerValue); }

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static float GetFloatValue(UPARAM(ref) FParameter& InParameter) { return InParameter.GetFloatValue(); }

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetFloatValue(UPARAM(ref) FParameter& InParameter, float InFloatValue) { InParameter.SetFloatValue(InFloatValue); }

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FString GetStringValue(UPARAM(ref) FParameter& InParameter) { return InParameter.GetStringValue(); }

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetStringValue(UPARAM(ref) FParameter& InParameter, const FString& InStringValue) { InParameter.SetStringValue(InStringValue); }

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FText GetTextValue(UPARAM(ref) FParameter& InParameter) { return InParameter.GetTextValue(); }

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetTextValue(UPARAM(ref) FParameter& InParameter, const FText InTextValue) { InParameter.SetTextValue(InTextValue); }

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static bool GetBooleanValue(UPARAM(ref) FParameter& InParameter) { return InParameter.GetBooleanValue(); }

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetBooleanValue(UPARAM(ref) FParameter& InParameter, bool InBooleanValue) { InParameter.SetBooleanValue(InBooleanValue); }

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FVector GetVectorValue(UPARAM(ref) FParameter& InParameter) { return InParameter.GetVectorValue(); }

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetVectorValue(UPARAM(ref) FParameter& InParameter, const FVector& InVectorValue) { InParameter.SetVectorValue(InVectorValue); }

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FRotator GetRotatorValue(UPARAM(ref) FParameter& InParameter) { return InParameter.GetRotatorValue(); }

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetRotatorValue(UPARAM(ref) FParameter& InParameter, const FRotator& InRotatorValue) { InParameter.SetRotatorValue(InRotatorValue); }

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FColor GetColorValue(UPARAM(ref) FParameter& InParameter) { return InParameter.GetColorValue(); }

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetColorValue(UPARAM(ref) FParameter& InParameter, const FColor& InColorValue) { InParameter.SetColorValue(InColorValue); }

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static UClass* GetClassValue(UPARAM(ref) FParameter& InParameter) { return InParameter.GetClassValue(); }

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetClassValue(UPARAM(ref) FParameter& InParameter, UClass* InClassValue) { InParameter.SetClassValue(InClassValue); }

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static UObject* GetObjectValue(UPARAM(ref) FParameter& InParameter) { return InParameter.GetObjectValue(); }

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetObjectValue(UPARAM(ref) FParameter& InParameter, UObject* InObjectValue) { InParameter.SetObjectValue(InObjectValue); }

	//////////////////////////////////////////////////////////////////////////
	/// Parameters
public:
	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static bool HasParameter(UPARAM(ref) FParameters& InParameters, FName InName, bool bEnsured = true) { return InParameters.HasParameter(InName, bEnsured); }

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void AddParameter(UPARAM(ref) FParameters& InParameters, FName InName, FParameter InParameter){ InParameters.AddParameter(InName, InParameter); }

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetParameter(UPARAM(ref) FParameters& InParameters, FName InName, FParameter InParameter){ InParameters.SetParameter(InName, InParameter); }

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FParameter GetParameter(UPARAM(ref) FParameters& InParameters, FName InName, bool bEnsured = true){ return InParameters.GetParameter(InName, bEnsured); }

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<FParameter> GetParameters(UPARAM(ref) FParameters& InParameters, FName InName, bool bEnsured = true){ return InParameters.GetParameters(InName, bEnsured); }

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void RemoveParameter(UPARAM(ref) FParameters& InParameters, FName InName){ InParameters.RemoveParameter(InName); }

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void RemoveParameters(UPARAM(ref) FParameters& InParameters, FName InName){ InParameters.RemoveParameters(InName); }

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void ClearAllParameter(UPARAM(ref) FParameters& InParameters) { InParameters.ClearAllParameter(); }
};

