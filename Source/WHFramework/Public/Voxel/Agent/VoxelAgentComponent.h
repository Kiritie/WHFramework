#pragma once
#include "Components/ActorComponent.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Voxel/Interaction/VoxelEditTypes.h"
#include "VoxelAgentComponent.generated.h"
class UVoxelModule;class APlayerController;class UVoxelModuleNetworkComponent;
UCLASS(ClassGroup=(Voxel),meta=(BlueprintSpawnableComponent))
class WHFRAMEWORK_API UVoxelAgentComponent:public UActorComponent
{
    GENERATED_BODY()
public:
    UVoxelAgentComponent();
    virtual void BeginPlay()override;
    virtual void EndPlay(const EEndPlayReason::Type Reason)override;
    virtual void TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* Function)override;
    UFUNCTION(BlueprintCallable) void SetAgentEnabled(bool bEnabled);
    UFUNCTION(BlueprintCallable) void BindController(APlayerController* Controller);
    UFUNCTION(BlueprintCallable) bool TraceVoxel(FVoxelHitResult& Out)const;
    UFUNCTION(BlueprintCallable) void BeginBreak();
    UFUNCTION(BlueprintCallable) void EndBreak();
    UFUNCTION(BlueprintCallable) bool PlaceSelected();
    UFUNCTION(BlueprintCallable) bool PlaceItem(const FVoxelItem& Item);
    UFUNCTION(BlueprintCallable) bool UseTarget();
    UFUNCTION(BlueprintCallable) bool TakeContainerSlot(int32 ContainerSlot,int32 Count);
    UFUNCTION(BlueprintCallable) bool PutContainerSlot(int32 ContainerSlot,int32 Count);
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bEnableStreaming=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bEnableNonPlayerSource=false;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bUseControllerView=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bCreativeInStandalone=false;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bGateCharacterUntilCollision=true;
    UPROPERTY(EditAnywhere,Category="Voxel|Movement") bool bUseSmallVoxelStepHeight=false;
private:
    bool View(FVector& Origin,FVector& Direction)const;
    bool MakeIntent(EVoxelEditAction Action,FVoxelEditIntent& Out)const;
    bool Send(FVoxelEditIntent Intent);
    void RefreshSource();
    void RefreshStepHeight();
    void GateCharacter();
    void OnReply(const FVoxelEditReply& Reply);
    TWeakObjectPtr<APlayerController> BoundController;
    TWeakObjectPtr<UVoxelModule> Module;
    TWeakObjectPtr<UVoxelModuleNetworkComponent> Network;
    FGuid SourceId;
    FDelegateHandle ReplyHandle;
    bool bHeld=false,bBreakActive=false,bGated=false;
    uint8 PreviousMoveMode=0,PreviousCustomMode=0;
    float PreviousStepHeight=-1.f;
    FIntVector BreakTarget=FIntVector::ZeroValue;
    double LastPulse=-10;
    uint64 NextLocalID=1,LastRequest=0;
};
