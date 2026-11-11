#pragma once

#include "Procedure/ProcedureModule.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ProcedureModuleStatics.generated.h"

UCLASS()
class WHFRAMEWORK_API UProcedureModuleStatics : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable)
    static bool StartMain(UWorld* InWorld);
    UFUNCTION(BlueprintCallable)
    static void StopMain(UWorld* InWorld);
    UFUNCTION(BlueprintCallable)
    static bool SwitchProcedure(UWorld* InWorld, UProcedureBase* InProcedure);
    UFUNCTION(BlueprintCallable)
    static bool SwitchProcedureFrom(UWorld* InWorld, const FProcedureActivationToken& Token, UProcedureBase* InProcedure);
    UFUNCTION(BlueprintPure)
    static bool CanSwitchProcedure(UWorld* InWorld, UProcedureBase* InProcedure);
    UFUNCTION(BlueprintCallable)
    static bool SwitchProcedureByIndex(UWorld* InWorld, int32 InIndex);
    UFUNCTION(BlueprintPure)
    static bool CanSwitchProcedureByIndex(UWorld* InWorld, int32 InIndex);
    UFUNCTION(BlueprintCallable)
    static bool SwitchProcedureByClass(UWorld* InWorld, TSubclassOf<UProcedureBase> InClass);
    UFUNCTION(BlueprintPure)
    static bool CanSwitchProcedureByClass(UWorld* InWorld, TSubclassOf<UProcedureBase> InClass);
    UFUNCTION(BlueprintCallable)
    static bool SwitchFirstProcedure(UWorld* InWorld);
    UFUNCTION(BlueprintCallable)
    static bool SwitchLastProcedure(UWorld* InWorld);
    UFUNCTION(BlueprintCallable)
    static bool SwitchPreProcedure(UWorld* InWorld);
    UFUNCTION(BlueprintCallable)
    static bool SwitchNextProcedure(UWorld* InWorld);
    UFUNCTION(BlueprintCallable)
    static bool Advance(UWorld* InWorld, FName Output = "Out");
    UFUNCTION(BlueprintPure)
    static bool CanAdvance(UWorld* InWorld, FName Output = "Out");
    UFUNCTION(BlueprintCallable)
    static bool AdvanceFrom(UWorld* InWorld, const FProcedureActivationToken& Token, FName Output = "Out");
    UFUNCTION(BlueprintCallable)
    static void GuideCurrentProcedure(UWorld* InWorld);
    UFUNCTION(BlueprintPure)
    static UProcedureAsset* GetCurrentProcedureAsset(UWorld* InWorld);
    UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
    static UProcedureBase* GetCurrentProcedure(UWorld* InWorld, TSubclassOf<UProcedureBase> InClass = nullptr);
    UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
    static UProcedureBase* GetProcedureByClass(UWorld* InWorld, TSubclassOf<UProcedureBase> InClass);
    UFUNCTION(BlueprintPure)
    static bool HasProcedureByClass(UWorld* InWorld, TSubclassOf<UProcedureBase> InClass);
    UFUNCTION(BlueprintPure)
    static bool IsCurrentProcedure(UWorld* InWorld, UProcedureBase* InProcedure);
    UFUNCTION(BlueprintPure)
    static bool IsCurrentProcedureClass(UWorld* InWorld, TSubclassOf<UProcedureBase> InClass);
    UFUNCTION(BlueprintPure)
    static bool HasProcedureByIndex(UWorld* InWorld, int32 InIndex);
    UFUNCTION(BlueprintPure)
    static bool IsCurrentProcedureIndex(UWorld* InWorld, int32 InIndex);
    UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
    static UProcedureBase* GetProcedureByIndex(UWorld* InWorld, int32 InIndex, TSubclassOf<UProcedureBase> InClass = nullptr);
    template <class T>
    static T* GetCurrentProcedure(UWorld* World) { UProcedureModule* M = UProcedureModule::GetPtr(World); return M ? M->GetCurrentProcedure<T>() : nullptr; }
    template <class T>
    static T* GetProcedureByClass(UWorld* World, TSubclassOf<UProcedureBase> Class = T::StaticClass()) { UProcedureModule* M = UProcedureModule::GetPtr(World); return M ? M->GetProcedureByClass<T>(Class) : nullptr; }
    template <class T>
    static T* GetProcedureByIndex(UWorld* World, int32 Index) { UProcedureModule* M = UProcedureModule::GetPtr(World); return M ? M->GetProcedureByIndex<T>(Index) : nullptr; }
    template <class T>
    static bool HasProcedureByClass(UWorld* World) { UProcedureModule* M = UProcedureModule::GetPtr(World); return M && M->HasProcedureByClass<T>(); }
    template <class T>
    static bool IsCurrentProcedureClass(UWorld* World) { UProcedureModule* M = UProcedureModule::GetPtr(World); return M && M->IsCurrentProcedureClass<T>(); }
    template <class T>
    static bool SwitchProcedureByClass(UWorld* World, TSubclassOf<UProcedureBase> Class = T::StaticClass()) { UProcedureModule* M = UProcedureModule::GetPtr(World); return M && M->SwitchProcedureByClass(Class); }
    template <class T>
    static bool CanSwitchProcedureByClass(UWorld* World, TSubclassOf<UProcedureBase> Class = T::StaticClass()) { UProcedureModule* M = UProcedureModule::GetPtr(World); return M && M->CanSwitchProcedureByClass(Class); }
};