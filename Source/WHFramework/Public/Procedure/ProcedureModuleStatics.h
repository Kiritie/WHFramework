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
    static bool StartMain();
    UFUNCTION(BlueprintCallable)
    static void StopMain();
    UFUNCTION(BlueprintCallable)
    static bool SwitchProcedure(UProcedureBase* InProcedure);
    UFUNCTION(BlueprintCallable)
    static bool SwitchProcedureFrom(const FProcedureActivationToken& Token, UProcedureBase* InProcedure);
    UFUNCTION(BlueprintPure)
    static bool CanSwitchProcedure(UProcedureBase* InProcedure);
    UFUNCTION(BlueprintCallable)
    static bool SwitchProcedureByIndex(int32 InIndex);
    UFUNCTION(BlueprintPure)
    static bool CanSwitchProcedureByIndex(int32 InIndex);
    UFUNCTION(BlueprintCallable)
    static bool SwitchProcedureByClass(TSubclassOf<UProcedureBase> InClass);
    UFUNCTION(BlueprintPure)
    static bool CanSwitchProcedureByClass(TSubclassOf<UProcedureBase> InClass);
    UFUNCTION(BlueprintCallable)
    static bool SwitchFirstProcedure();
    UFUNCTION(BlueprintCallable)
    static bool SwitchLastProcedure();
    UFUNCTION(BlueprintCallable)
    static bool SwitchPreProcedure();
    UFUNCTION(BlueprintCallable)
    static bool SwitchNextProcedure();
    UFUNCTION(BlueprintCallable)
    static bool Advance(FName Output = "Out");
    UFUNCTION(BlueprintPure)
    static bool CanAdvance(FName Output = "Out");
    UFUNCTION(BlueprintCallable)
    static bool AdvanceFrom(const FProcedureActivationToken& Token, FName Output = "Out");
    UFUNCTION(BlueprintCallable)
    static void GuideCurrentProcedure();
    UFUNCTION(BlueprintPure)
    static UProcedureAsset* GetCurrentProcedureAsset();
    UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
    static UProcedureBase* GetCurrentProcedure(TSubclassOf<UProcedureBase> InClass = nullptr);
    UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
    static UProcedureBase* GetProcedureByClass(TSubclassOf<UProcedureBase> InClass);
    UFUNCTION(BlueprintPure)
    static bool HasProcedureByClass(TSubclassOf<UProcedureBase> InClass);
    UFUNCTION(BlueprintPure)
    static bool IsCurrentProcedure(UProcedureBase* InProcedure);
    UFUNCTION(BlueprintPure)
    static bool IsCurrentProcedureClass(TSubclassOf<UProcedureBase> InClass);
    UFUNCTION(BlueprintPure)
    static bool HasProcedureByIndex(int32 InIndex);
    UFUNCTION(BlueprintPure)
    static bool IsCurrentProcedureIndex(int32 InIndex);
    UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
    static UProcedureBase* GetProcedureByIndex(int32 InIndex, TSubclassOf<UProcedureBase> InClass = nullptr);
    template <class T>
    static T* GetCurrentProcedure() { return UProcedureModule::Get().GetCurrentProcedure<T>(); }
    template <class T>
    static T* GetProcedureByClass(TSubclassOf<UProcedureBase> InClass = T::StaticClass())
    {
        return UProcedureModule::Get().GetProcedureByClass<T>(InClass);
    }
    template <class T>
    static T* GetProcedureByIndex(int32 InIndex) { return UProcedureModule::Get().GetProcedureByIndex<T>(InIndex); }
    template <class T>
    static bool HasProcedureByClass() { return UProcedureModule::Get().HasProcedureByClass<T>(); }
    template <class T>
    static bool IsCurrentProcedureClass() { return UProcedureModule::Get().IsCurrentProcedureClass<T>(); }
    template <class T>
    static bool SwitchProcedureByClass(TSubclassOf<UProcedureBase> InClass = T::StaticClass())
    {
        return UProcedureModule::Get().SwitchProcedureByClass(InClass);
    }
    template <class T>
    static bool CanSwitchProcedureByClass(TSubclassOf<UProcedureBase> InClass = T::StaticClass())
    {
        return UProcedureModule::Get().CanSwitchProcedureByClass(InClass);
    }
};
