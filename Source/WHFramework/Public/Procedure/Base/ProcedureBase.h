#pragma once

#include "Camera/Mode/CameraModeBase.h"
#include "Camera/CameraModuleTypes.h"
#include "Common/Base/WHObject.h"
#include "Procedure/ProcedureModuleTypes.h"
#include "ProcedureBase.generated.h"

class UProcedureAsset;
class UProcedureModule;
class ACameraShotAnchor;

UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced, hidecategories = (Default))
class WHFRAMEWORK_API UProcedureBase : public UWHObject
{
    GENERATED_BODY()

    friend class UProcedureModule;

public:
    UProcedureBase();
    virtual UWorld* GetWorld() const override;

#if WITH_EDITOR
    virtual void OnGenerate();
    virtual void OnUnGenerate();
    virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnStateChanged")
    void K2_OnStateChanged(EProcedureState InProcedureState);
    UFUNCTION()
    virtual void OnStateChanged(EProcedureState InProcedureState);
    UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnInitialize")
    void K2_OnInitialize();
    UFUNCTION()
    virtual void OnInitialize();
    UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnEnter")
    void K2_OnEnter(UProcedureBase* InLastProcedure);
    UFUNCTION()
    virtual void OnEnter(UProcedureBase* InLastProcedure);
    UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnRefresh")
    void K2_OnRefresh();
    UFUNCTION()
    virtual void OnRefresh();
    UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnGuide")
    void K2_OnGuide();
    UFUNCTION()
    virtual void OnGuide();
    UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnLeave")
    void K2_OnLeave(UProcedureBase* InNextProcedure);
    UFUNCTION()
    virtual void OnLeave(UProcedureBase* InNextProcedure);
    virtual void OnShutdown();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void Guide();
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool IsCurrent();
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void Switch();
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void UnSwitch();
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void SwitchLast();
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void SwitchNext();
    UFUNCTION(BlueprintCallable)
    void SwitchPre();
    UFUNCTION(BlueprintCallable)
    bool Advance(FName Output = "Out");
    UFUNCTION(BlueprintPure)
    bool CanAdvance(FName Output = "Out") const;
    UFUNCTION(BlueprintPure)
    FProcedureActivationToken CaptureActivation() const;
    UFUNCTION(BlueprintPure)
    UProcedureModule* GetProcedureModule() const;
    UFUNCTION(BlueprintPure)
    UProcedureAsset* GetProcedureAsset() const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGuid NodeId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ProcedureName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText ProcedureDisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = "true"))
    FText ProcedureDescription;
#if WITH_EDITORONLY_DATA
    UPROPERTY()
    FVector2D GraphPosition = FVector2D::ZeroVector;
#endif
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly)
    EProcedureState ProcedureState = EProcedureState::None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EProcedureGuideType ProcedureGuideType = EProcedureGuideType::None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01"))
    float ProcedureGuideIntervalTime = 1.f;
    UPROPERTY(BlueprintAssignable)
    FOnProcedureStateChanged OnProcedureStateChanged;
    UFUNCTION(BlueprintPure)
    EProcedureState GetProcedureState() const { return ProcedureState; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation Target")
    TSoftObjectPtr<AActor> OperationTarget;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
    bool bApplyCameraAction = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bApplyCameraAction"), Category = "Camera")
    TSubclassOf<UCameraModeBase> CameraModeClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bApplyCameraAction"), Category = "Camera")
    FCameraConfigOverride CameraOverride;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bApplyCameraAction"), Category = "Camera")
    TSoftObjectPtr<ACameraShotAnchor> CameraAnchor;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bApplyCameraAction"), Category = "Camera")
    FCameraTransitionParams CameraTransition;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
    bool bRestoreDefaultCameraOnLeave = false;
    FCameraConfigOverrideHandle CameraOverrideHandle;

    UFUNCTION(BlueprintCallable)
    void ApplyCameraAction();
    template <class T>
    T* GetOperationTarget() const { return Cast<T>(GetOperationTarget(T::StaticClass())); }
    UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
    AActor* GetOperationTarget(TSubclassOf<AActor> InClass) const;
    UFUNCTION(BlueprintCallable)
    void SetOperationTarget(AActor* InOperationTarget, bool bResetCameraView = true);

protected:
    FTimerHandle StartGuideTimerHandle;

private:
    void ScheduleGuide();

    UPROPERTY(Transient)
    TObjectPtr<UProcedureModule> RuntimeOwner;
    FProcedureActivationToken Activation;
};
