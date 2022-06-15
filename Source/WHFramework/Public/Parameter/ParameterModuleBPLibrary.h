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
	//////////////////////////////////////////////////////////////////////////
	/// Parameter
	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static bool HasParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetParameter(FName InName, FParameter InParameter);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FParameter GetParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<FParameter> GetParameters(FName InName, bool bEnsured = true);

	//////////////////////////////////////////////////////////////////////////
	/// Integer
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetIntegerParameter(FName InName, int32 InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static int32 GetIntegerParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<int32> GetIntegerParameters(FName InName, bool bEnsured = true);
	
	//////////////////////////////////////////////////////////////////////////
	/// Float
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetFloatParameter(FName InName, float InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static float GetFloatParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<float> GetFloatParameters(FName InName, bool bEnsured = true);
	
	//////////////////////////////////////////////////////////////////////////
	/// String
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetStringParameter(FName InName, const FString& InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FString GetStringParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<FString> GetStringParameters(FName InName, bool bEnsured = true);
		
	//////////////////////////////////////////////////////////////////////////
	/// Text
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetTextParameter(FName InName, const FText InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FText GetTextParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<FText> GetTextParameters(FName InName, bool bEnsured = true);

	//////////////////////////////////////////////////////////////////////////
	/// Boolean
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetBooleanParameter(FName InName, bool InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static bool GetBooleanParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<bool> GetBooleanParameters(FName InName, bool bEnsured = true);
	
	//////////////////////////////////////////////////////////////////////////
	/// Vector
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetVectorParameter(FName InName, FVector InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FVector GetVectorParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<FVector> GetVectorParameters(FName InName, bool bEnsured = true);
	
	//////////////////////////////////////////////////////////////////////////
	/// Rotator
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetRotatorParameter(FName InName, FRotator InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FRotator GetRotatorParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<FRotator> GetRotatorParameters(FName InName, bool bEnsured = true);
		
	//////////////////////////////////////////////////////////////////////////
	/// Color
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetColorParameter(FName InName, const FColor& InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FColor GetColorParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<FColor> GetColorParameters(FName InName, bool bEnsured = true);

	//////////////////////////////////////////////////////////////////////////
	/// Class
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetClassParameter(FName InName, UClass* InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static UClass* GetClassParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<UClass*> GetClassParameters(FName InName, bool bEnsured = true);
	
	//////////////////////////////////////////////////////////////////////////
	/// Object
	UFUNCTION(BlueprintCallable, Category = "ParameterModuleBPLibrary")
	static void SetObjectParameter(FName InName, UObject* InValue);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static UObject* GetObjectParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static TArray<UObject*> GetObjectParameters(FName InName, bool bEnsured = true);

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
	static FText GetTextValue(const FParameter& InParameter);

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
