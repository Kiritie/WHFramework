// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/VoxelTree.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Datas/VoxelTreeData.h"

UVoxelTree::UVoxelTree()
{
	TreePart = EVoxelTreePart::None;
	TreeHeight = 0;
	LeavesHeight = 0;
	LeavesWidth = 0;
}

void UVoxelTree::OnReset_Implementation()
{
	Super::OnReset_Implementation();

	TreePart = EVoxelTreePart::None;
	RootIndex = FIndex::ZeroIndex;
	TreeHeight = 0;
	LeavesHeight = 0;
	LeavesWidth = 0;
}

void UVoxelTree::LoadData(const FString& InData)
{
	TArray<FString> Datas;
	InData.ParseIntoArray(Datas, TEXT(";"));
	TreePart = (EVoxelTreePart)FCString::Atoi(*Datas[0]);
	switch (TreePart)
	{
		case EVoxelTreePart::Root:
		{
			TreeHeight = FCString::Atoi(*Datas[1]);
			LeavesHeight = FCString::Atoi(*Datas[2]);
			LeavesWidth = FCString::Atoi(*Datas[3]);
			break;
		}
		case EVoxelTreePart::Bole:
		case EVoxelTreePart::Leaves:
		{
			RootIndex = UVoxelModuleBPLibrary::NumberToVoxelIndex(FCString::Atoi(*Datas[1]));
			break;
		}
		default: break;
	}
}

FString UVoxelTree::ToData()
{
	switch (TreePart)
	{
		case EVoxelTreePart::Root:
		{
			return FString::Printf(TEXT("%d;%d;%d;%d"), TreePart, TreeHeight, LeavesHeight, LeavesWidth);
		}
		case EVoxelTreePart::Bole:
		case EVoxelTreePart::Leaves:
		{
			return FString::Printf(TEXT("%d;%d"), TreePart, UVoxelModuleBPLibrary::VoxelIndexToNumber(RootIndex));
		}
		default: break;
	}
	return TEXT("");
}

FString UVoxelTree::BuildData(EVoxelTreePart InTreePart)
{
	if(AVoxelChunk* Owner = GetOwner())
	{
		TreePart = InTreePart;
		switch (TreePart)
		{
			case EVoxelTreePart::Root:
			{
				const auto& TreeData = GetData<UVoxelTreeData>();
				
				const FRandomStream& RandomStream = AVoxelModule::Get()->GetWorldData().RandomStream;
				TreeHeight = RandomStream.RandRange(TreeData.TreeHeight.X, TreeData.TreeHeight.Y);
				LeavesHeight = RandomStream.RandRange(TreeData.LeavesHeight.X, TreeData.LeavesHeight.Y);
				LeavesWidth = RandomStream.FRand() < 0.5f ? TreeData.LeavesWidth.X : TreeData.LeavesWidth.Y;
				
				TMap<FIndex, FVoxelItem> VoxelItems;
				VoxelItems.Emplace(GetIndex(), FVoxelItem(TreeData.VoxelType, FIndex::ZeroIndex, nullptr, ToData()));
				for(int32 OffsetZ = TreeHeight - LeavesHeight / 2; OffsetZ <= TreeHeight + LeavesHeight / 2; OffsetZ++)
				{
					for(int32 OffsetX = -LeavesWidth / 2; OffsetX <= LeavesWidth / 2; OffsetX++)
					{
						for(int32 OffsetY = -LeavesWidth / 2; OffsetY <= LeavesWidth / 2; OffsetY++)
						{
							VoxelItems.Emplace(GetIndex() + FIndex(OffsetX, OffsetY, OffsetZ - 1), FVoxelItem(TreeData.LeavesData->VoxelType, FIndex::ZeroIndex, nullptr, BuildData(EVoxelTreePart::Leaves)));
						}
					}
				}
				DON_WITHINDEX(TreeHeight - 1, i,
					VoxelItems.Emplace(GetIndex() + FIndex(0, 0, i + 1), FVoxelItem(TreeData.VoxelType, FIndex::ZeroIndex, nullptr, BuildData(EVoxelTreePart::Bole)));
				)
				Owner->SetVoxelComplex(VoxelItems);
				break;
			}
			case EVoxelTreePart::Bole:
			case EVoxelTreePart::Leaves:
			{
				RootIndex = GetIndex();
				break;
			}
			default: break;
		}
	}
	return ToData();
}

void UVoxelTree::OnGenerate(IVoxelAgentInterface* InAgent)
{
	Super::OnGenerate(InAgent);
}

void UVoxelTree::OnDestroy(IVoxelAgentInterface* InAgent)
{
	Super::OnDestroy(InAgent);

	if(AVoxelChunk* Owner = GetOwner())
	{
		const FIndex VoxelIndex = GetIndex();
		switch (TreePart)
		{
			case EVoxelTreePart::Root:
			{
				TMap<FIndex, FVoxelItem> VoxelItems;
				DON_WITHINDEX(TreeHeight - 1, i,
					VoxelItems.Emplace(GetIndex() + FIndex(0, 0, i + 1), FVoxelItem::Empty);
				)
				for(int32 OffsetZ = TreeHeight - LeavesHeight / 2; OffsetZ <= TreeHeight + LeavesHeight / 2; OffsetZ++)
				{
					for(int32 OffsetX = -LeavesWidth / 2; OffsetX <= LeavesWidth / 2; OffsetX++)
					{
						for(int32 OffsetY = -LeavesWidth / 2; OffsetY <= LeavesWidth / 2; OffsetY++)
						{
							VoxelItems.Emplace(GetIndex() + FIndex(OffsetX, OffsetY, OffsetZ - 1), FVoxelItem::Empty);
						}
					}
				}
				for(auto& Iter : TMap(VoxelItems))
				{
					if(Owner->GetVoxel<UVoxelTree>(Iter.Key).RootIndex != VoxelIndex)
					{
						VoxelItems.Remove(Iter.Key);
					}
				}
				Owner->SetVoxelComplex(VoxelItems, true, false, InAgent);
				break;
			}
			default: break;
		}
	}
}

void UVoxelTree::OnAgentHit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentHit(InAgent, InHitResult);
}

void UVoxelTree::OnAgentEnter(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentEnter(InAgent, InHitResult);
}

void UVoxelTree::OnAgentStay(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentStay(InAgent, InHitResult);
}

void UVoxelTree::OnAgentExit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentExit(InAgent, InHitResult);
}

bool UVoxelTree::OnAgentInteract(IVoxelAgentInterface* InAgent, EInputInteractAction InInteractAction, const FVoxelHitResult& InHitResult)
{
	return Super::OnAgentInteract(InAgent, InInteractAction, InHitResult);
}
