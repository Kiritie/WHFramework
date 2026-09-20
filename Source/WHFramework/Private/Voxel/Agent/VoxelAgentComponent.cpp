#include "Voxel/Agent/VoxelAgentComponent.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Voxels/VoxelItemBridge.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Network/VoxelModuleNetworkComponent.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
UVoxelAgentComponent::UVoxelAgentComponent(){PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PrePhysics;}
void UVoxelAgentComponent::BeginPlay()
{
    Super::BeginPlay();Module=&UVoxelModule::Get();
    if(auto*P=Cast<APawn>(GetOwner()))BindController(Cast<APlayerController>(P->GetController()));
    if(auto*C=Cast<ACharacter>(GetOwner()))if(C->GetCharacterMovement())C->GetCharacterMovement()->AddTickPrerequisiteComponent(this);
}
void UVoxelAgentComponent::BindController(APlayerController*PC)
{
    if(PC&&PC->GetWorld()!=GetWorld())return;if(Network.IsValid())Network->OnIntentReply.Remove(ReplyHandle);
    BoundController=PC;Network=PC?PC->FindComponentByClass<UVoxelModuleNetworkComponent>():nullptr;
    if(Network.IsValid())ReplyHandle=Network->OnIntentReply.AddUObject(this,&UVoxelAgentComponent::OnReply);
}
bool UVoxelAgentComponent::View(FVector&O,FVector&D)const
{
    if(!GetOwner())return false;FRotator R;
    if(bUseControllerView&&BoundController.IsValid()&&BoundController->IsLocalController())BoundController->GetPlayerViewPoint(O,R);
    else if(auto*C=GetOwner()->FindComponentByClass<UCameraComponent>()){O=C->GetComponentLocation();R=C->GetComponentRotation();}
    else GetOwner()->GetActorEyesViewPoint(O,R);D=R.Vector();return !O.ContainsNaN()&&!D.ContainsNaN();
}
bool UVoxelAgentComponent::TraceVoxel(FVoxelHitResult&O)const
{
    O=FVoxelHitResult();auto*M=Module.Get();FVector P,D;if(!M||!M->IsReady()||!View(P,D))return false;
    auto H=M->Trace(P,D);O.bNeedsData=H.Status==EVoxelTraceStatus::NeedsData;if(H.Status!=EVoxelTraceStatus::Hit)return false;
    O.bHit=true;O.Index=H.Index;O.PlacementIndex=H.PlacementIndex;O.Point=H.Point;O.Normal=H.Normal;
    return FVoxelItemBridge::ToItem(*M->GetRegistry(),H.State,1,O.VoxelItem);
}
bool UVoxelAgentComponent::MakeIntent(EVoxelEditAction A,FVoxelEditIntent&O)const
{
    auto*M=Module.Get();if(!M||!M->IsReady()||!View(O.Origin,O.Direction))return false;
    auto H=M->Trace(O.Origin,O.Direction);if(H.Status!=EVoxelTraceStatus::Hit)return false;
    O.Action=A;O.ExpectedTarget=H.Index;const FVoxelSectionKey OldKey=VoxelCoord::Section(H.Index);const auto*S=M->GetRuntime()->FindSection({OldKey.X,OldKey.Y,OldKey.Z});if(!S)return false;O.ExpectedRevision=S->CommittedRevision;
    AActor*InventoryOwner=BoundController.IsValid()&&BoundController->GetPawn()?BoundController->GetPawn():GetOwner();
    if(auto*Agent=Cast<IAbilityInventoryAgentInterface>(InventoryOwner))if(auto*I=Agent->GetInventory())
        if(auto*Slot=I->GetSelectedSlot(ESlotSplitType::Shortcut)){O.InventorySlot=Slot->GetSlotIndex();O.ExpectedItemID=Slot->GetItem().ID;}
    return true;
}
bool UVoxelAgentComponent::Send(FVoxelEditIntent I)
{
    auto*M=Module.Get();if(!M||!M->IsReady())return false;
    if(GetWorld()->GetNetMode()==NM_Standalone)
    {
        I.RequestId=NextLocalID==MAX_uint64?0:NextLocalID++;if(!I.RequestId)return false;LastRequest=I.RequestId;
        auto R=M->ExecuteIntent(BoundController.Get(),GetOwner(),I,bCreativeInStandalone);OnReply(R);return R.Code!=EVoxelEditCode::Rejected;
    }
    if(!Network.IsValid()||!Network->IsSessionReady())return false;I.RequestId=Network->AllocateRequestId();if(!I.RequestId)return false;
    LastRequest=I.RequestId;return Network->SubmitIntent(I);
}
void UVoxelAgentComponent::BeginBreak(){bHeld=true;LastPulse=-10;}
void UVoxelAgentComponent::EndBreak()
{
    bHeld=false;if(bBreakActive){FVoxelEditIntent I;I.Action=EVoxelEditAction::BreakCancel;I.ExpectedTarget=BreakTarget;View(I.Origin,I.Direction);Send(I);}bBreakActive=false;
}
bool UVoxelAgentComponent::PlaceSelected(){FVoxelEditIntent I;return MakeIntent(EVoxelEditAction::Place,I)&&Send(I);}
bool UVoxelAgentComponent::PlaceItem(const FVoxelItem&Item)
{FVoxelEditIntent I;if(!Item.IsValid()||!MakeIntent(EVoxelEditAction::Place,I))return false;I.ExpectedItemID=Item.VoxelAssetID;return Send(I);}
bool UVoxelAgentComponent::UseTarget(){FVoxelEditIntent I;return MakeIntent(EVoxelEditAction::Use,I)&&Send(I);}
bool UVoxelAgentComponent::TakeContainerSlot(int32 Slot,int32 Count)
{FVoxelEditIntent I;if(Slot<0||Slot>=27||Count<=0||Count>100000||!MakeIntent(EVoxelEditAction::ContainerTake,I))return false;I.ContainerSlot=Slot;I.Count=Count;return Send(I);}
bool UVoxelAgentComponent::PutContainerSlot(int32 Slot,int32 Count)
{FVoxelEditIntent I;if(Slot<0||Slot>=27||Count<=0||Count>100000||!MakeIntent(EVoxelEditAction::ContainerPut,I))return false;I.ContainerSlot=Slot;I.Count=Count;return Send(I);}
void UVoxelAgentComponent::OnReply(const FVoxelEditReply&R)
{if(R.RequestId==LastRequest&&R.Code!=EVoxelEditCode::Pending)bBreakActive=false;}
void UVoxelAgentComponent::RefreshSource()
{
	UVoxelModule* VoxelModule = Module.Get();
	if (!VoxelModule)
	{
		return;
	}

	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		APlayerController* PlayerController =
			Cast<APlayerController>(Pawn->GetController());
		if (BoundController.Get() != PlayerController)
		{
			BindController(PlayerController);
		}
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	bool bActive =
		bEnableStreaming &&
		(!Pawn || Pawn->IsPlayerControlled() || bEnableNonPlayerSource);

	if (GetWorld()->GetNetMode() != NM_Standalone)
	{
		bActive =
			bActive &&
			BoundController.IsValid() &&
			BoundController->IsLocalController();
	}

	if (!bActive)
	{
		if (SourceId.IsValid())
		{
			VoxelModule->UnregisterSource(SourceId);
		}
		SourceId.Invalidate();
		return;
	}

	FVoxelStreamingSource Source;
	FVector ViewOrigin;
	FVector ViewDirection;

	if (!View(ViewOrigin, ViewDirection) ||
		!VoxelCoord::FromWorld(
			GetOwner()->GetActorLocation(),
			VoxelModule->BlockSize(),
			Source.Center))
	{
		return;
	}

	Source.Id = SourceId.IsValid() ? SourceId : FGuid::NewGuid();
	Source.Direction = ViewDirection;
	Source.ExactRadius = FMath::Max(0, ExactRadiusCells);
	Source.CollisionRadius = FMath::Max(0, CollisionRadiusCells);
	Source.SimulationRadius = FMath::Max(0, SimulationRadiusCells);
	Source.VerticalExactRadius = FMath::Max(0, VerticalExactRadiusCells);

	if (BoundController.IsValid())
	{
		Source.VerticalFovDegrees =
			BoundController->PlayerCameraManager
				? BoundController->PlayerCameraManager->GetFOVAngle()
				: 90.0f;

		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;
		BoundController->GetViewportSize(ViewportWidth, ViewportHeight);
		Source.ViewportHeightPixels = FMath::Max(1, ViewportHeight);
	}

	Source.bCollision = true;
	Source.bSimulation = SimulationRadiusCells > 0;
	Source.bRender = GetWorld()->GetNetMode() != NM_DedicatedServer;

	if (!SourceId.IsValid())
	{
		SourceId = VoxelModule->RegisterSource(this, Source);
	}
	else if (!VoxelModule->UpdateSource(SourceId, Source))
	{
		SourceId.Invalidate();
	}
}
void UVoxelAgentComponent::RefreshStepHeight()
{
    auto*C=Cast<ACharacter>(GetOwner());auto*M=Module.Get();if(!C||!C->GetCharacterMovement())return;
    auto*Move=C->GetCharacterMovement();
    if(bUseSmallVoxelStepHeight&&M&&M->IsReady())
    {
        if(PreviousStepHeight<0.f)PreviousStepHeight=Move->MaxStepHeight;
        Move->MaxStepHeight=FMath::Clamp(float(M->BlockSize())+5.f,20.f,40.f);
    }
    else if(PreviousStepHeight>=0.f)
    {
        Move->MaxStepHeight=PreviousStepHeight;PreviousStepHeight=-1.f;
    }
}

void UVoxelAgentComponent::GateCharacter()
{
    auto*C=Cast<ACharacter>(GetOwner());auto*M=Module.Get();if(!C||!C->IsPlayerControlled()||!bGateCharacterUntilCollision||!C->GetCharacterMovement())return;
    bool Ready=M&&M->IsReady();FIntVector Min,Max;
    if(Ready)
    {
        FBox Bounds=C->GetCapsuleComponent()->Bounds.GetBox();Bounds.Min-=FVector(1);Bounds.Max+=FVector(1);
        Ready=VoxelCoord::FromWorld(Bounds.Min,M->BlockSize(),Min)&&VoxelCoord::FromWorld(Bounds.Max,M->BlockSize(),Max);
        auto A=VoxelCoord::Section(Min),B=VoxelCoord::Section(Max);
        if(B.X-A.X>8||B.Y-A.Y>8||B.Z-A.Z>8)Ready=false;
        for(int32 Z=A.Z;Ready&&Z<=B.Z;++Z)for(int32 Y=A.Y;Ready&&Y<=B.Y;++Y)for(int32 X=A.X;Ready&&X<=B.X;++X)
        {Ready=M->IsCollisionReady({X,Y,Z});}
    }
    auto*Movement=C->GetCharacterMovement();
    if(!Ready&&!bGated){PreviousMoveMode=uint8(Movement->MovementMode);PreviousCustomMode=Movement->CustomMovementMode;bGated=true;Movement->StopMovementImmediately();Movement->DisableMovement();}
    else if(Ready&&bGated){bGated=false;Movement->SetMovementMode(EMovementMode(PreviousMoveMode),PreviousCustomMode);}
}
void UVoxelAgentComponent::TickComponent(float D,ELevelTick T,FActorComponentTickFunction*F)
{
    Super::TickComponent(D,T,F);
    RefreshSource();RefreshStepHeight();GateCharacter();double Now=FPlatformTime::Seconds();
    if(bHeld&&Now-LastPulse>=.1)
    {
        LastPulse=Now;FVoxelEditIntent I;if(!MakeIntent(EVoxelEditAction::BreakPulse,I)){if(bBreakActive)EndBreak();return;}
        if(!bBreakActive||BreakTarget!=I.ExpectedTarget){I.Action=EVoxelEditAction::BreakBegin;BreakTarget=I.ExpectedTarget;bBreakActive=true;}
        if(!Send(I))bBreakActive=false;
    }
}
void UVoxelAgentComponent::EndPlay(const EEndPlayReason::Type R)
{
    EndBreak();if(Module.IsValid())Module->UnregisterSource(SourceId);if(Network.IsValid())Network->OnIntentReply.Remove(ReplyHandle);
    if(auto*C=Cast<ACharacter>(GetOwner()))if(C->GetCharacterMovement()&&PreviousStepHeight>=0.f)C->GetCharacterMovement()->MaxStepHeight=PreviousStepHeight;
    PreviousStepHeight=-1.f;Super::EndPlay(R);
}

void UVoxelAgentComponent::SetAgentEnabled(bool Enabled)
{
    if(!Enabled)
    {
        EndBreak();if(Module.IsValid()&&SourceId.IsValid())Module->UnregisterSource(SourceId);SourceId.Invalidate();BindController(nullptr);
        if(bGated)if(auto*C=Cast<ACharacter>(GetOwner()))if(auto*M=C->GetCharacterMovement())M->SetMovementMode(EMovementMode(PreviousMoveMode),PreviousCustomMode);
        bGated=false;
    }
    bEnableStreaming=Enabled;SetComponentTickEnabled(Enabled);
}
