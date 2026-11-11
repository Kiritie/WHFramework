#pragma once

#include "FSMModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Parameter/ParameterTypes.h"
#include "FSMModuleStatics.generated.h"

class UFSMComponent;
class UFiniteStateBase;
class UWorld;

UCLASS()
class WHFRAMEWORK_API UFSMModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="FSMModuleStatics")
	static void RegisterFSM(UFSMComponent* InFSM);
	UFUNCTION(BlueprintCallable, Category="FSMModuleStatics")
	static void UnregisterFSM(UFSMComponent* InFSM);
	UFUNCTION(BlueprintPure, meta=(WorldContext="InWorld"), Category="FSMModuleStatics")
	static bool HasFSMGroup(UWorld* InWorld, FName InGroupName);
	UFUNCTION(BlueprintPure, meta=(WorldContext="InWorld"), Category="FSMModuleStatics")
	static FFSMGroup GetFSMGroup(UWorld* InWorld, FName InGroupName);
	UFUNCTION(BlueprintCallable, meta=(WorldContext="InWorld", AutoCreateRefTerm="InParams"), Category="FSMModuleStatics")
	static void SwitchFSMGroupStateByIndex(UWorld* InWorld, FName InGroupName, int32 InStateIndex, const TArray<FParameter>& InParams);
	UFUNCTION(BlueprintCallable, meta=(WorldContext="InWorld", AutoCreateRefTerm="InParams"), Category="FSMModuleStatics")
	static void SwitchFSMGroupStateByName(UWorld* InWorld, FName InGroupName, FName InStateName, const TArray<FParameter>& InParams);
	UFUNCTION(BlueprintCallable, meta=(WorldContext="InWorld", AutoCreateRefTerm="InParams"), Category="FSMModuleStatics")
	static void SwitchFSMGroupStateByClass(UWorld* InWorld, FName InGroupName, TSubclassOf<UFiniteStateBase> InStateClass, const TArray<FParameter>& InParams);
	UFUNCTION(BlueprintCallable, meta=(WorldContext="InWorld", AutoCreateRefTerm="InParams"), Category="FSMModuleStatics")
	static void SwitchFSMGroupDefaultState(UWorld* InWorld, FName InGroupName, const TArray<FParameter>& InParams);
	UFUNCTION(BlueprintCallable, meta=(WorldContext="InWorld", AutoCreateRefTerm="InParams"), Category="FSMModuleStatics")
	static void SwitchFSMGroupFinalState(UWorld* InWorld, FName InGroupName, const TArray<FParameter>& InParams);
	UFUNCTION(BlueprintCallable, meta=(WorldContext="InWorld", AutoCreateRefTerm="InParams"), Category="FSMModuleStatics")
	static void SwitchFSMGroupLastState(UWorld* InWorld, FName InGroupName, const TArray<FParameter>& InParams);
	UFUNCTION(BlueprintCallable, meta=(WorldContext="InWorld", AutoCreateRefTerm="InParams"), Category="FSMModuleStatics")
	static void SwitchFSMGroupNextState(UWorld* InWorld, FName InGroupName, const TArray<FParameter>& InParams);
};
