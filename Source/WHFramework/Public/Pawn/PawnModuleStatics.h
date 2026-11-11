#pragma once

#include "PawnModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PawnModuleStatics.generated.h"

class APawnBase;
class APlayerController;
class UWorld;

UCLASS()
class WHFRAMEWORK_API UPawnModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, meta=(DeterminesOutputType="InClass"), Category="PawnModuleStatics")
	static APawnBase* GetCurrentPawn(APlayerController* InPlayer, TSubclassOf<APawnBase> InClass);
	UFUNCTION(BlueprintPure, meta=(WorldContext="InWorld"), Category="PawnModuleStatics")
	static TArray<APawnBase*> GetAllPawn(UWorld* InWorld);
	UFUNCTION(BlueprintCallable, Category="PawnModuleStatics")
	static bool SwitchPawn(APlayerController* InPlayer, APawnBase* InPawn, bool bResetCamera = true, bool bInstant = false);
	UFUNCTION(BlueprintCallable, Category="PawnModuleStatics")
	static bool SwitchPawnByClass(APlayerController* InPlayer, TSubclassOf<APawnBase> InClass, bool bResetCamera = true, bool bInstant = false);
	UFUNCTION(BlueprintCallable, Category="PawnModuleStatics")
	static bool SwitchPawnByName(APlayerController* InPlayer, FName InName, bool bResetCamera = true, bool bInstant = false);
	UFUNCTION(BlueprintPure, meta=(WorldContext="InWorld"), Category="PawnModuleStatics")
	static bool HasPawnByClass(UWorld* InWorld, TSubclassOf<APawnBase> InClass);
	UFUNCTION(BlueprintPure, meta=(WorldContext="InWorld"), Category="PawnModuleStatics")
	static bool HasPawnByName(UWorld* InWorld, FName InName);
	UFUNCTION(BlueprintPure, meta=(WorldContext="InWorld", DeterminesOutputType="InClass"), Category="PawnModuleStatics")
	static APawnBase* GetPawnByClass(UWorld* InWorld, TSubclassOf<APawnBase> InClass);
	UFUNCTION(BlueprintPure, meta=(WorldContext="InWorld"), Category="PawnModuleStatics")
	static APawnBase* GetPawnByName(UWorld* InWorld, FName InName);
	UFUNCTION(BlueprintCallable, Category="PawnModuleStatics")
	static void AddPawnToList(APawnBase* InPawn);
	UFUNCTION(BlueprintCallable, Category="PawnModuleStatics")
	static void RemovePawnFromList(APawnBase* InPawn);
};
