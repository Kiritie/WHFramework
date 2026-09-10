#pragma once

#include "ParameterModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ParameterModuleStatics.generated.h"

class FProperty;

UCLASS()
class WHFRAMEWORK_API UParameterModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "ParameterModule")
	static bool HasGlobalParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "ParameterModule")
	static void SetGlobalParameter(FName InName, FParameter InParameter);

	UFUNCTION(BlueprintPure, Category = "ParameterModule")
	static FParameter GetGlobalParameter(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "ParameterModule")
	static TArray<FParameter> GetGlobalParameters(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "ParameterModule")
	static void RemoveGlobalParameter(FName InName);

	UFUNCTION(BlueprintCallable, Category = "ParameterModule")
	static void RemoveGlobalParameters(FName InName);

	UFUNCTION(BlueprintCallable, Category = "ParameterModule")
	static void ClearAllGlobalParameter();

	UFUNCTION(BlueprintPure, CustomThunk, meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "Value", AdvancedDisplay = "Description", AutoCreateRefTerm = "Description"), Category = "ParameterModule")
	static FParameter MakeParameter(const int32& Value, const FText& Description = FText::GetEmpty());

	DECLARE_FUNCTION(execMakeParameter);

	UFUNCTION(BlueprintPure, CustomThunk, meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "Value"), Category = "ParameterModule")
	static void GetParameterValue(const FParameter& Parameter, int32& Value, bool& Success);

	DECLARE_FUNCTION(execGetParameterValue);

	UFUNCTION(BlueprintCallable, CustomThunk, meta = (CustomStructureParam = "Value"), Category = "ParameterModule")
	static void SetParameterValue(UPARAM(ref) FParameter& Parameter, const int32& Value);

	DECLARE_FUNCTION(execSetParameterValue);

	UFUNCTION(BlueprintPure, CustomThunk, meta = (CustomStructureParam = "Value"), Category = "ParameterModule")
	static bool IsParameterType(const FParameter& Parameter, const int32& Value);

	DECLARE_FUNCTION(execIsParameterType);

	UFUNCTION(BlueprintPure, Category = "ParameterModule")
	static FText GetDescription(UPARAM(ref) const FParameter& Parameter) { return Parameter.GetDescription(); }

	UFUNCTION(BlueprintCallable, Category = "ParameterModule")
	static void SetDescription(UPARAM(ref) FParameter& Parameter, const FText& Description) { Parameter.SetDescription(Description); }

	UFUNCTION(BlueprintPure, Category = "ParameterModule")
	static bool HasParameter(UPARAM(ref) const FParameterSets& ParameterSets, FName Name, bool bEnsured = true) { return ParameterSets.HasParameter(Name, bEnsured); }

	UFUNCTION(BlueprintCallable, Category = "ParameterModule")
	static void SetParameter(UPARAM(ref) FParameterSets& ParameterSets, FName Name, FParameter Parameter) { ParameterSets.SetParameter(Name, Parameter); }

	UFUNCTION(BlueprintPure, Category = "ParameterModule")
	static FParameter GetParameter(UPARAM(ref) const FParameterSets& ParameterSets, FName Name, bool bEnsured = true) { return ParameterSets.GetParameter(Name, bEnsured); }

	UFUNCTION(BlueprintPure, Category = "ParameterModule")
	static TArray<FParameter> GetParameters(UPARAM(ref) const FParameterSets& ParameterSets, FName Name, bool bEnsured = true) { return ParameterSets.GetParameters(Name, bEnsured); }

	UFUNCTION(BlueprintCallable, Category = "ParameterModule")
	static void RemoveParameter(UPARAM(ref) FParameterSets& ParameterSets, FName Name)
	{
		ParameterSets.RemoveParameter(Name);
	}

	UFUNCTION(BlueprintCallable, Category = "ParameterModule")
	static void RemoveParameters(UPARAM(ref) FParameterSets& ParameterSets, FName Name)
	{
		ParameterSets.RemoveParameters(Name);
	}

	UFUNCTION(BlueprintCallable, Category = "ParameterModule")
	static void ClearAllParameter(UPARAM(ref) FParameterSets& ParameterSets)
	{
		ParameterSets.ClearAllParameter();
	}

	UFUNCTION(BlueprintCallable, Category = "ParameterModule")
	static FParameterMap MakeParameterMap(const TMap<FString, FString>& ParameterMap);

	UFUNCTION(BlueprintCallable, Category = "ParameterModule")
	static FParameterMap& AddParameterMapValue(UPARAM(ref) FParameterMap& ParameterMap, const FString& Key, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "ParameterModule")
	static FParameterMap& SetParameterMapValue(UPARAM(ref) FParameterMap& ParameterMap, const FString& Key, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "ParameterModule")
	static FParameterMap& RemoveParameterMapKey(UPARAM(ref) FParameterMap& ParameterMap, const FString& Key);

	UFUNCTION(BlueprintCallable, Category = "ParameterModule")
	static FParameterMap& ClearParameterMap(UPARAM(ref) FParameterMap& ParameterMap);

	UFUNCTION(BlueprintPure, Category = "ParameterModule")
	static bool ContainsParameterMapKey(UPARAM(ref) const FParameterMap& ParameterMap, const FString& Key) { return ParameterMap.Contains(Key); }

	UFUNCTION(BlueprintPure, Category = "ParameterModule")
	static FString GetParameterMapValue(UPARAM(ref) const FParameterMap& ParameterMap, const FString& Key) { return ParameterMap.Get(Key); }

	UFUNCTION(BlueprintPure, Category = "ParameterModule")
	static const TMap<FString, FString>& GetParameterMapSource(UPARAM(ref) const FParameterMap& ParameterMap) { return ParameterMap.GetSource(); }

	UFUNCTION(BlueprintPure, Category = "ParameterModule")
	static int32 GetParameterMapNum(UPARAM(ref) const FParameterMap& ParameterMap) { return ParameterMap.GetNum(); }

	UFUNCTION(BlueprintPure, Category = "ParameterModule")
	static FString ParameterMapToString(UPARAM(ref) const FParameterMap& ParameterMap) { return ParameterMap.ToString(); }

	UFUNCTION(BlueprintPure, Category = "ParameterModule")
	static FString ParameterMapToJsonString(UPARAM(ref) const FParameterMap& ParameterMap) { return ParameterMap.ToJsonString(); }

private:
	static bool SetValueFromProperty(FParameter& Parameter, const FProperty* Property, const void* ValuePtr);
	static bool GetValueToProperty(const FParameter& Parameter, const FProperty* Property, void* ValuePtr);
	static bool IsValuePropertyType(const FParameter& Parameter, const FProperty* Property);
};
