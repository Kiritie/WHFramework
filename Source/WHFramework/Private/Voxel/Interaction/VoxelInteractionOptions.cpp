#include "Voxel/Interaction/VoxelInteractionOptions.h"

#include "Voxel/Agent/VoxelAgentInterface.h"
#include "Voxel/Voxels/Auxiliary/VoxelInteractAuxiliary.h"
#include "Voxel/Voxels/VoxelSwitch.h"

namespace
{
	AVoxelInteractAuxiliary* GetVoxelTarget(const FInteractionContext& InContext)
	{
		return Cast<AVoxelInteractAuxiliary>(InContext.Target);
	}

	IVoxelAgentInterface* GetVoxelInteractor(const FInteractionContext& InContext)
	{
		return Cast<IVoxelAgentInterface>(InContext.Interactor);
	}

	UVoxelSwitch* GetVoxelSwitch(const FInteractionContext& InContext)
	{
		AVoxelInteractAuxiliary* Target = GetVoxelTarget(InContext);
		return Target ? &Target->GetVoxelItem(true).GetVoxel<UVoxelSwitch>() : nullptr;
	}
}

bool UInteractionCondition_VoxelInteract::Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	const AVoxelInteractAuxiliary* Target = GetVoxelTarget(InContext);
	return Target && GetVoxelInteractor(InContext) && !Target->IsInteracting();
}

bool UInteractionAction_VoxelInteract::Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	AVoxelInteractAuxiliary* Target = GetVoxelTarget(InContext);
	IVoxelAgentInterface* Interactor = GetVoxelInteractor(InContext);
	return Target && Interactor && Target->SetInteracting(true, Interactor);
}

UInteractionOption_VoxelInteract::UInteractionOption_VoxelInteract()
{
	OptionTag = GameplayTags::Voxel_Interaction_Option_Interact;
	DisplayName = NSLOCTEXT("Interaction", "VoxelInteract", "交互");
	VisibilityConditions.Add(CreateDefaultSubobject<UInteractionCondition_VoxelInteract>(TEXT("InteractCondition")));
	Actions.Add(CreateDefaultSubobject<UInteractionAction_VoxelInteract>(TEXT("InteractAction")));
}

bool UInteractionCondition_VoxelUnInteract::Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	const AVoxelInteractAuxiliary* Target = GetVoxelTarget(InContext);
	return Target && GetVoxelInteractor(InContext) && Target->IsInteracting();
}

bool UInteractionAction_VoxelUnInteract::Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	AVoxelInteractAuxiliary* Target = GetVoxelTarget(InContext);
	IVoxelAgentInterface* Interactor = GetVoxelInteractor(InContext);
	return Target && Interactor && Target->SetInteracting(false, Interactor);
}

UInteractionOption_VoxelUnInteract::UInteractionOption_VoxelUnInteract()
{
	OptionTag = GameplayTags::Voxel_Interaction_Option_UnInteract;
	DisplayName = NSLOCTEXT("Interaction", "VoxelUnInteract", "退出");
	VisibilityConditions.Add(CreateDefaultSubobject<UInteractionCondition_VoxelUnInteract>(TEXT("UnInteractCondition")));
	Actions.Add(CreateDefaultSubobject<UInteractionAction_VoxelUnInteract>(TEXT("UnInteractAction")));
}

bool UInteractionCondition_VoxelOpen::Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	const UVoxelSwitch* Voxel = GetVoxelSwitch(InContext);
	return Voxel && GetVoxelInteractor(InContext) && !Voxel->IsOpened();
}

bool UInteractionAction_VoxelOpen::Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	UVoxelSwitch* Voxel = GetVoxelSwitch(InContext);
	IVoxelAgentInterface* Interactor = GetVoxelInteractor(InContext);
	if (!Voxel || !Interactor || Voxel->IsOpened()) return false;
	Voxel->Open(Interactor);
	return true;
}

UInteractionOption_VoxelOpen::UInteractionOption_VoxelOpen()
{
	OptionTag = GameplayTags::Voxel_Interaction_Option_Open;
	DisplayName = NSLOCTEXT("Interaction", "VoxelOpen", "打开");
	VisibilityConditions.Add(CreateDefaultSubobject<UInteractionCondition_VoxelOpen>(TEXT("OpenCondition")));
	Actions.Add(CreateDefaultSubobject<UInteractionAction_VoxelOpen>(TEXT("OpenAction")));
}

void UInteractionAction_VoxelOpen::Finish_Implementation(const FInteractionContext& InContext, EInteractionActionState InState) const
{
	if (InState == EInteractionActionState::Completed || !IsValid(InContext.Target)) return;
	if (UVoxelSwitch* Voxel = GetVoxelSwitch(InContext); Voxel && Voxel->IsOpened()) Voxel->Close(GetVoxelInteractor(InContext));
}

bool UInteractionCondition_VoxelClose::Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	const UVoxelSwitch* Voxel = GetVoxelSwitch(InContext);
	return Voxel && GetVoxelInteractor(InContext) && Voxel->IsOpened();
}

bool UInteractionAction_VoxelClose::Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	UVoxelSwitch* Voxel = GetVoxelSwitch(InContext);
	IVoxelAgentInterface* Interactor = GetVoxelInteractor(InContext);
	if (!Voxel || !Interactor || !Voxel->IsOpened()) return false;
	Voxel->Close(Interactor);
	return true;
}

UInteractionOption_VoxelClose::UInteractionOption_VoxelClose()
{
	OptionTag = GameplayTags::Voxel_Interaction_Option_Close;
	DisplayName = NSLOCTEXT("Interaction", "VoxelClose", "关闭");
	VisibilityConditions.Add(CreateDefaultSubobject<UInteractionCondition_VoxelClose>(TEXT("CloseCondition")));
	Actions.Add(CreateDefaultSubobject<UInteractionAction_VoxelClose>(TEXT("CloseAction")));
}
