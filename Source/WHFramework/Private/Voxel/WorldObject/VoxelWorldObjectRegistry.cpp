#include "Voxel/WorldObject/VoxelWorldObjectRegistry.h"

#include "Misc/SecureHash.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"

FGuid FVoxelWorldObjectInstance::StableId() const
{
	const FString Identity = FString::Printf(TEXT("%s/%d/%d/%d"), *DefinitionId.ToString(), Anchor.X, Anchor.Y, Anchor.Z);
	const FTCHARToUTF8 Bytes(*Identity);
	FMD5 Hash;
	Hash.Update(reinterpret_cast<const uint8*>(Bytes.Get()), Bytes.Length());
	uint8 Digest[16];
	Hash.Final(Digest);
	uint32 Words[4];
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Words[Index] = uint32(Digest[Index * 4]) | (uint32(Digest[Index * 4 + 1]) << 8) |
			(uint32(Digest[Index * 4 + 2]) << 16) | (uint32(Digest[Index * 4 + 3]) << 24);
	}
	return FGuid(Words[0], Words[1], Words[2], Words[3]);
}

FIntVector FVoxelWorldObjectRegistry::Rotate(const FIntVector& InOffset, uint8 InYaw)
{
	switch (InYaw & 3)
	{
		case 1: return FIntVector(-InOffset.Y, InOffset.X, InOffset.Z);
		case 2: return FIntVector(-InOffset.X, -InOffset.Y, InOffset.Z);
		case 3: return FIntVector(InOffset.Y, -InOffset.X, InOffset.Z);
		default: return InOffset;
	}
}

FVoxelBlockState FVoxelWorldObjectRegistry::PartState(const FVoxelWorldObjectDefinitionRuntime& InDefinition, int32 InPart, uint8 InYaw, uint16 InFlags)
{
	return {InDefinition.Types[InPart], uint16(InDefinition.Parts[InPart].State | InYaw | (InFlags & InDefinition.ToggleMask))};
}

void FVoxelWorldObjectRegistry::Reset()
{
	Definitions.Reset();
	TypeDefinitions.Reset();
	MaxMaterializationRadius = 0.;
}

bool FVoxelWorldObjectRegistry::Build(const FVoxelRegistrySnapshot& InBlocks, const TArray<UVoxelWorldObjectDefinition*>& InDefinitions, FString& OutError)
{
	FVoxelWorldObjectRegistry Candidate;
	for (UVoxelWorldObjectDefinition* Source : InDefinitions)
	{
		if (!Source || Source->DefinitionId.IsNone() || Candidate.Definitions.Contains(Source->DefinitionId) ||
			Source->Footprint.IsEmpty() || Source->Footprint.Num() > 64 || Source->Footprint[0].Offset != FIntVector::ZeroValue ||
			Source->ToggleMask < 0 || Source->ToggleMask > MAX_uint16 || (Source->ToggleMask & VoxelState::FacingMask))
		{
			OutError = TEXT("Invalid world object identity, anchor or footprint size");
			return false;
		}
		FVoxelWorldObjectDefinitionRuntime Definition;
		Definition.Id = Source->DefinitionId;
		Definition.UseAction = Source->UseAction;
		Definition.Source = Source;
		Definition.Parts = Source->Footprint;
		Definition.ToggleMask = uint16(Source->ToggleMask);
		if (!FMath::IsFinite(Source->MaterializationRadius) || Source->MaterializationRadius < 0.f)
		{
			OutError = TEXT("World object materialization radius must be finite and nonnegative");
			return false;
		}
		if (Source->Presentation == EVoxelWorldObjectPresentation::PresenterActor || Source->Presentation == EVoxelWorldObjectPresentation::StaticMesh)
		{
			Candidate.MaxMaterializationRadius = FMath::Max(Candidate.MaxMaterializationRadius, double(Source->MaterializationRadius));
		}
		TSet<FIntVector> Offsets;
		TSet<uint32> Signatures;
		for (int32 Index = 0; Index < Definition.Parts.Num(); ++Index)
		{
			const FVoxelWorldObjectPart& Part = Definition.Parts[Index];
			const FVoxelRuntimeDefinition* Block = InBlocks.Find(Part.BlockName);
			if (!Block || Block->TypeId == 0 || (Index > 0 && Block->EntityKind != 0) || Offsets.Contains(Part.Offset) ||
				Part.Offset.GetAbsMax() > 32 || Part.State < 0 || Part.State > MAX_uint16 ||
				(Part.State & (VoxelState::FacingMask | Definition.ToggleMask)) || Candidate.TypeDefinitions.Contains(Block->TypeId))
			{
				OutError = TEXT("Invalid world object part; only the anchor may own an entity");
				return false;
			}
			const uint32 Signature = FVoxelBlockState{Block->TypeId, uint16(Part.State)}.Pack();
			if (Signatures.Contains(Signature))
			{
				OutError = TEXT("Object parts need distinct type/state identities to resolve the anchor");
				return false;
			}
			for (uint8 Yaw = 0; Yaw < 4; ++Yaw)
			{
				if (!InBlocks.IsValid({Block->TypeId, uint16(Part.State | Yaw)}) ||
					!InBlocks.IsValid({Block->TypeId, uint16(Part.State | Yaw | Definition.ToggleMask)}))
				{
					OutError = TEXT("Object part state is not supported by its shape");
					return false;
				}
			}
			Offsets.Add(Part.Offset);
			Signatures.Add(Signature);
			Definition.Types.Add(Block->TypeId);
		}
		for (uint16 Type : Definition.Types)
		{
			Candidate.TypeDefinitions.Add(Type, Definition.Id);
		}
		Candidate.Definitions.Add(Definition.Id, MoveTemp(Definition));
	}
	*this = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}

const FVoxelWorldObjectDefinitionRuntime* FVoxelWorldObjectRegistry::Find(FName InId) const
{
	return Definitions.Find(InId);
}

const FVoxelWorldObjectDefinitionRuntime* FVoxelWorldObjectRegistry::Find(uint16 InType) const
{
	const FName* Id = TypeDefinitions.Find(InType);
	return Id ? Find(*Id) : nullptr;
}

bool FVoxelWorldObjectRegistry::Resolve(const FVoxelWorldRuntime& InWorld, const FIntVector& InCell, FVoxelWorldObjectInstance& OutObject, FString& OutError) const
{
	FVoxelBlockState State;
	if (!InWorld.TryGetBlock(InCell, State))
	{
		OutError = TEXT("World object section is unavailable");
		return false;
	}
	const FVoxelWorldObjectDefinitionRuntime* Definition = Find(State.TypeId);
	if (!Definition)
	{
		OutError = TEXT("Block has no world object definition");
		return false;
	}
	const uint8 Yaw = uint8(State.State & 3);
	for (int32 Part = 0; Part < Definition->Parts.Num(); ++Part)
	{
		if (PartState(*Definition, Part, Yaw, State.State) != State)
		{
			continue;
		}
		const FIntVector Anchor = InCell - Rotate(Definition->Parts[Part].Offset, Yaw);
		for (int32 Index = 0; Index < Definition->Parts.Num(); ++Index)
		{
			FVoxelBlockState Actual;
			if (!InWorld.TryGetBlock(Anchor + Rotate(Definition->Parts[Index].Offset, Yaw), Actual) ||
				Actual != PartState(*Definition, Index, Yaw, State.State))
			{
				OutError = TEXT("World object footprint is incomplete or unavailable");
				return false;
			}
		}
		OutObject.DefinitionId = Definition->Id;
		OutObject.Anchor = Anchor;
		OutObject.Yaw = Yaw;
		OutObject.State = PartState(*Definition, 0, Yaw, State.State);
		OutError.Reset();
		return true;
	}
	OutError = TEXT("World object part identity does not match its definition");
	return false;
}
