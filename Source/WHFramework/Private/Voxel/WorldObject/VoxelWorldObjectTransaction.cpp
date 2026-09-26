#include "Voxel/WorldObject/VoxelWorldObjectTransaction.h"

#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Save/VoxelBlockEntityCodec.h"

const FVoxelBlockEntityState* FVoxelWorldObjectTransaction::FindEntity(const FVoxelWorldRuntime& InWorld, const FIntVector& InAnchor)
{
	const FVoxelSectionKey Key = VoxelCoord::Section(InAnchor);
	const FVoxelSection* Section = InWorld.FindSection(FIntVector(Key.X, Key.Y, Key.Z));
	return Section && Section->Status == EVoxelSectionStatus::DataReady ? Section->Entities.Find(VoxelCoord::Linear(VoxelCoord::Local(InAnchor))) : nullptr;
}

bool FVoxelWorldObjectTransaction::Build(const FVoxelWorldRuntime& InWorld, const FVoxelRegistrySnapshot& InBlocks, const FVoxelShapeRegistry& InShapes, const FVoxelWorldObjectRegistry& InObjects, const FVoxelTraceResult& InHit, EVoxelEditAction InAction, uint16 InPlaceType, const FVector& InView, FVoxelInteractionPlan& OutPlan, FString& OutError)
{
	if (!InWorld.IsServer() || InHit.Status != EVoxelTraceStatus::Hit || InHit.bStartedInside || !InBlocks.IsValid(InHit.State))
	{
		OutError = TEXT("Invalid authoritative world object request");
		return false;
	}
	FVoxelWorldObjectInstance Object;
	const FVoxelWorldObjectDefinitionRuntime* Definition = nullptr;
	FVoxelInteractionPlan Plan;
	if (InAction == EVoxelEditAction::Place)
	{
		Definition = InObjects.Find(InPlaceType);
		if (!Definition || Definition->Types[0] != InPlaceType)
		{
			OutError = TEXT("Only the object anchor item can place an object");
			return false;
		}
		Object.DefinitionId = Definition->Id;
		Object.Anchor = InBlocks.Find(InHit.State.TypeId)->bReplaceable ? InHit.Index : InHit.PlacementIndex;
		Object.Yaw = uint8((FMath::RoundToInt(FMath::Atan2(InView.Y, InView.X) / (PI * .5)) + 4) & 3);
		Plan.Cost = 1;
	}
	else
	{
		if (!InObjects.Resolve(InWorld, InHit.Index, Object, OutError))
		{
			return false;
		}
		Definition = InObjects.Find(Object.DefinitionId);
	}
	Plan.ObjectAnchor = Object.Anchor;
	Plan.ObjectId = Object.StableId();
	Plan.ObjectAction = InAction == EVoxelEditAction::Use ? Definition->UseAction : NAME_None;
	const FVoxelRuntimeDefinition* AnchorBlock = InBlocks.Find(Definition->Types[0]);
	if (InAction == EVoxelEditAction::BreakPulse)
	{
		if (!AnchorBlock->bBreakable)
		{
			OutError = TEXT("World object cannot be destroyed");
			return false;
		}
		const FVoxelBlockEntityState* Entity = FindEntity(InWorld, Object.Anchor);
		if (Entity && Entity->Kind == 1)
		{
			TArray<FVoxelItemStack> Items;
			if (!FVoxelBlockEntityCodec::DecodeContainer(*Entity, Items))
			{
				return false;
			}
			for (const FVoxelItemStack& Item : Items)
			{
				if (Item.Count > 0)
				{
					OutError = TEXT("Empty the container before destroying it");
					return false;
				}
			}
		}
		Plan.DropID = AnchorBlock->DropAssetID;
		Plan.DropCount = AnchorBlock->DropCount;
	}
	else if (InAction != EVoxelEditAction::Place && InAction != EVoxelEditAction::Use)
	{
		OutError = TEXT("Unsupported world object action");
		return false;
	}
	for (int32 Index = 0; Index < Definition->Parts.Num(); ++Index)
	{
		FVoxelCellEdit Edit;
		Edit.Position = Object.Anchor + FVoxelWorldObjectRegistry::Rotate(Definition->Parts[Index].Offset, Object.Yaw);
		if (!VoxelCoord::IsValid(Edit.Position) || !InWorld.TryGetBlock(Edit.Position, Edit.Expected))
		{
			OutError = TEXT("Complete footprint must be loaded before editing");
			return false;
		}
		if (InAction == EVoxelEditAction::Place)
		{
			const FVoxelRuntimeDefinition* Previous = InBlocks.Find(Edit.Expected.TypeId);
			if (!Previous || (!Edit.Expected.IsAir() && !Previous->bReplaceable) || InObjects.Find(Edit.Expected.TypeId) || FindEntity(InWorld, Edit.Position))
			{
				OutError = TEXT("Object footprint is occupied");
				return false;
			}
			Edit.Value = FVoxelWorldObjectRegistry::PartState(*Definition, Index, Object.Yaw);
		}
		else if (InAction == EVoxelEditAction::Use)
		{
			Edit.Value = Edit.Expected;
			Edit.Value.State ^= Definition->ToggleMask;
		}
		Plan.Cells.Add(Edit);
	}
	if (AnchorBlock->EntityKind && InAction != EVoxelEditAction::Use)
	{
		FVoxelEntityEdit Entity;
		Entity.Position = Object.Anchor;
		Entity.bRemove = InAction == EVoxelEditAction::BreakPulse;
		if (!Entity.bRemove && !FVoxelBlockEntityCodec::MakeDefault(AnchorBlock->EntityKind, Entity.Value, AnchorBlock->EntityVariant))
		{
			OutError = TEXT("World object entity codec is not registered");
			return false;
		}
		Plan.Entities.Add(MoveTemp(Entity));
	}
	OutPlan = MoveTemp(Plan);
	return true;
}

bool FVoxelWorldObjectTransaction::Validate(const FVoxelWorldRuntime& InWorld, const FVoxelRegistrySnapshot& InBlocks, const FVoxelShapeRegistry& InShapes, const FVoxelWorldObjectRegistry& InObjects, TArray<FVoxelCellEdit>& InOutCells, FString& OutError)
{
	TArray<FVoxelCellEdit> Cells = InOutCells;
	if (!FVoxelEditTransaction::ValidateBatch(InWorld, InBlocks, InShapes, Cells, OutError))
	{
		return false;
	}
	TMap<FIntVector, FVoxelBlockState> Final;
	TSet<FIntVector> Inspect;
	for (const FVoxelCellEdit& Cell : Cells)
	{
		Final.Add(Cell.Position, Cell.Value);
		Inspect.Add(Cell.Position);
		Inspect.Add(Cell.Position + FIntVector(0, 0, 1));
		if (InObjects.Find(Cell.Expected.TypeId))
		{
			FVoxelWorldObjectInstance Previous;
			if (!InObjects.Resolve(InWorld, Cell.Position, Previous, OutError))
			{
				return false;
			}
			const FVoxelWorldObjectDefinitionRuntime* Definition = InObjects.Find(Previous.DefinitionId);
			for (const FVoxelWorldObjectPart& Part : Definition->Parts)
			{
				Inspect.Add(Previous.Anchor + FVoxelWorldObjectRegistry::Rotate(Part.Offset, Previous.Yaw));
			}
		}
	}
	auto Read = [&](const FIntVector& Position, FVoxelBlockState& State)
	{
		if (const FVoxelBlockState* Value = Final.Find(Position))
		{
			State = *Value;
			return true;
		}
		return InWorld.TryGetBlock(Position, State);
	};
	for (const FIntVector& Position : Inspect)
	{
		FVoxelBlockState State;
		if (!Read(Position, State))
		{
			OutError = TEXT("Object support dependency is unavailable");
			return false;
		}
		const FVoxelWorldObjectDefinitionRuntime* Definition = InObjects.Find(State.TypeId);
		if (!Definition)
		{
			continue;
		}
		const uint8 Yaw = uint8(State.State & 3);
		bool bMatched = false;
		for (int32 Part = 0; Part < Definition->Parts.Num(); ++Part)
		{
			if (FVoxelWorldObjectRegistry::PartState(*Definition, Part, Yaw, State.State) != State)
			{
				continue;
			}
			bMatched = true;
			const FIntVector Anchor = Position - FVoxelWorldObjectRegistry::Rotate(Definition->Parts[Part].Offset, Yaw);
			for (int32 Index = 0; Index < Definition->Parts.Num(); ++Index)
			{
				const FIntVector Cell = Anchor + FVoxelWorldObjectRegistry::Rotate(Definition->Parts[Index].Offset, Yaw);
				FVoxelBlockState Actual;
				if (!Read(Cell, Actual) || Actual != FVoxelWorldObjectRegistry::PartState(*Definition, Index, Yaw, State.State))
				{
					OutError = TEXT("Edit would leave an incomplete object footprint");
					return false;
				}
				if (Definition->Parts[Index].bNeedsFloor)
				{
					FVoxelBlockState Support;
					const bool bLoaded = Read(Cell - FIntVector(0, 0, 1), Support);
					const FVoxelRuntimeDefinition* Block = bLoaded ? InBlocks.Find(Support.TypeId) : nullptr;
					if (!Block || !Block->bSolid || !(InShapes.Get(Block->Shape, Support.State).OcclusionMask & (1u << 4)))
					{
						OutError = TEXT("World object requires a solid floor");
						return false;
					}
				}
			}
			break;
		}
		if (!bMatched)
		{
			OutError = TEXT("Invalid object part state");
			return false;
		}
	}
	InOutCells = MoveTemp(Cells);
	return true;
}
