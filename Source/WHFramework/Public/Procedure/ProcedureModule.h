#pragma once

#include "Procedure/Base/ProcedureAsset.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Main/Base/ModuleBase.h"
#include "ProcedureModule.generated.h"

struct FEventSwitchProcedure;

UCLASS()
class WHFRAMEWORK_API UProcedureModule : public UModuleBase
{
    GENERATED_BODY()
    GENERATED_MODULE(UProcedureModule)

    friend class FProcedureModuleDetailsPanel;

public:
    UProcedureModule();
    ~UProcedureModule();
#if WITH_EDITOR
    virtual void OnGenerate() override;
    virtual void OnDestroy() override;
    void SetDefaultAsset(UProcedureAsset* InAsset);
#endif
    virtual void OnInitialize() override;
    virtual void OnPreparatory(EPhase InPhase) override;
    virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;
    virtual void OnPause() override;
    virtual void OnUnPause() override;
    virtual void OnTermination(EPhase InPhase) override;
    virtual FString GetModuleDebugMessage() override;

    bool IsExecutingCallback() const { return bDispatching; }
    bool IsSwitchPending() const { return bHasQueuedSwitch; }
    UFUNCTION(BlueprintCallable)
    bool StartMain();
    UFUNCTION(BlueprintCallable)
    void StopMain();

    UFUNCTION(BlueprintCallable)
    bool SwitchProcedure(UProcedureBase* InProcedure);
    UFUNCTION(BlueprintCallable)
    bool SwitchProcedureFrom(const FProcedureActivationToken& Token, UProcedureBase* InProcedure);
    UFUNCTION(BlueprintPure)
    bool CanSwitchProcedure(UProcedureBase* InProcedure) const;
    UFUNCTION(BlueprintCallable)
    bool SwitchProcedureByIndex(int32 InIndex);
    UFUNCTION(BlueprintPure)
    bool CanSwitchProcedureByIndex(int32 InIndex) const;
    UFUNCTION(BlueprintCallable)
    bool SwitchProcedureByClass(TSubclassOf<UProcedureBase> InClass);
    template <class T>
    bool SwitchProcedureByClass() { return SwitchProcedureByClass(T::StaticClass()); }
    UFUNCTION(BlueprintPure)
    bool CanSwitchProcedureByClass(TSubclassOf<UProcedureBase> InClass) const;
    template <class T>
    bool CanSwitchProcedureByClass() const { return CanSwitchProcedureByClass(T::StaticClass()); }
    UFUNCTION(BlueprintCallable)
    bool SwitchFirstProcedure();
    UFUNCTION(BlueprintCallable)
    bool SwitchLastProcedure();
    UFUNCTION(BlueprintCallable)
    bool SwitchPreProcedure();
    UFUNCTION(BlueprintCallable)
    bool SwitchNextProcedure();
    UFUNCTION(BlueprintPure)
    FProcedureActivationToken CaptureActivation() const;
    UFUNCTION(BlueprintCallable)
    bool Advance(FName Output = "Out");
    UFUNCTION(BlueprintCallable)
    bool AdvanceFrom(const FProcedureActivationToken& Token, FName Output = "Out");
    UFUNCTION(BlueprintPure)
    bool CanAdvance(FName Output = "Out") const;
    UFUNCTION(BlueprintPure)
    UProcedureBase* GetNextProcedure(FName Output = "Out") const;
    UFUNCTION(BlueprintPure)
    UProcedureBase* GetPreviousProcedure() const;
    UFUNCTION(BlueprintPure)
    UProcedureBase* GetProcedureById(FGuid InNodeId) const;
    UFUNCTION(BlueprintPure)
    bool HasProcedureByIndex(int32 InIndex) const;
    UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
    UProcedureBase* GetProcedureByIndex(int32 InIndex, TSubclassOf<UProcedureBase> InClass = nullptr) const;
    template <class T>
    T* GetProcedureByIndex(int32 InIndex) const { return Cast<T>(GetProcedureByIndex(InIndex)); }
    UFUNCTION(BlueprintPure)
    bool IsCurrentProcedureIndex(int32 InIndex) const;
    UFUNCTION(BlueprintPure)
    UProcedureBase* GetFirstProcedure() const;
    UFUNCTION(BlueprintCallable)
    void GuideCurrentProcedure();

    UFUNCTION(BlueprintPure)
    UProcedureAsset* GetDefaultAsset() const { return DefaultAsset; }
    UFUNCTION(BlueprintPure)
    UProcedureAsset* GetCurrentAsset() const { return CurrentAsset; }
    UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
    UProcedureBase* GetCurrentProcedure(TSubclassOf<UProcedureBase> InClass = nullptr) const;
    template <class T>
    T* GetCurrentProcedure() const { return Cast<T>(CurrentProcedure); }
    UFUNCTION(BlueprintPure)
    TArray<UProcedureBase*> GetProcedures() const;
    UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
    UProcedureBase* GetProcedureByClass(TSubclassOf<UProcedureBase> InClass) const;
    template <class T>
    T* GetProcedureByClass(TSubclassOf<UProcedureBase> InClass = T::StaticClass()) const
    {
        return Cast<T>(GetProcedureByClass(InClass));
    }
    UFUNCTION(BlueprintPure)
    bool HasProcedureByClass(TSubclassOf<UProcedureBase> InClass) const;
    template <class T>
    bool HasProcedureByClass(TSubclassOf<UProcedureBase> InClass = T::StaticClass()) const
    {
        return HasProcedureByClass(InClass);
    }
    UFUNCTION(BlueprintPure)
    bool IsCurrentProcedure(const UProcedureBase* InProcedure) const;
    UFUNCTION(BlueprintPure)
    bool IsCurrentProcedureClass(TSubclassOf<UProcedureBase> InClass) const;
    template <class T>
    bool IsCurrentProcedureClass() const { return IsCurrentProcedureClass(T::StaticClass()); }

protected:
    int32 InitializedInstanceCount = 0;
    UPROPERTY(EditAnywhere, Category = "ProcedureModule|Procedure Stats")
    TObjectPtr<UProcedureAsset> DefaultAsset;
    UPROPERTY(Transient, VisibleAnywhere, Category = "ProcedureModule|Procedure Stats")
    TObjectPtr<UProcedureAsset> CurrentAsset;
    UPROPERTY(Transient, VisibleAnywhere, Category = "ProcedureModule|Procedure Stats")
    TObjectPtr<UProcedureBase> CurrentProcedure;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UProcedureBase>> Instances;
    UPROPERTY(Transient)
    TMap<FGuid, TObjectPtr<UProcedureBase>> InstanceMap;
    UFUNCTION()
    void OnSwitchProcedure(UObject* InSender, const FEventSwitchProcedure& InEvent);

private:

    TMap<FGuid, FGuid> NextIds;
    TMap<FGuid, FGuid> PreviousIds;
    int64 ActivationSerial = 0;
    bool bMainStarted = false;
    bool bStopRequested = false;
    bool bDispatching = false;
    bool bStopping = false;
    bool bLeaving = false;
    bool bHasQueuedSwitch = false;
    FProcedureActivationToken QueuedToken;
    FGuid QueuedTarget;
    bool CommitSwitch(const FProcedureActivationToken& Token, UProcedureBase* Target);
    void Enter(UProcedureBase* Next);
};
