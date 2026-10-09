#include "Voxel/Generation/VoxelGenerationPlanningRead.h"

FVoxelGenerationConsumerScope*& FVoxelGenerationConsumerScope::ActiveScopeSlot()
{
	static thread_local FVoxelGenerationConsumerScope* Scope = nullptr;
	return Scope;
}

FVoxelGenerationConsumerScope::FVoxelGenerationConsumerScope(const FVoxelGenerationConsumerKey& InConsumer)
	: Consumer(InConsumer)
	, Previous(ActiveScopeSlot())
{
	ActiveScopeSlot() = this;
}

FVoxelGenerationConsumerScope::~FVoxelGenerationConsumerScope()
{
	check(ActiveScopeSlot() == this);
	ActiveScopeSlot() = Previous;
}

const FVoxelGenerationConsumerKey* FVoxelGenerationConsumerScope::GetConsumerKey()
{
	return ActiveScopeSlot() && ActiveScopeSlot()->Consumer.IsValid() ? &ActiveScopeSlot()->Consumer : nullptr;
}

FVoxelGenerationPlanningReadScope*& FVoxelGenerationPlanningReadScope::ActiveScopeSlot()
{
	static thread_local FVoxelGenerationPlanningReadScope* Scope = nullptr;
	return Scope;
}

FVoxelGenerationPlanningReadScope::FVoxelGenerationPlanningReadScope(const FObserver* InObserver,
	const FVoxelTaskStamp* InStamp, const TAtomic<bool>* InCancel, const FVoxelGenerationConsumerKey* InConsumer)
	: Observer(InObserver)
	, Stamp(InStamp)
	, Cancel(InCancel)
	, Consumer(InConsumer)
	, Previous(ActiveScopeSlot())
{
	ActiveScopeSlot() = this;
}

FVoxelGenerationPlanningReadScope::~FVoxelGenerationPlanningReadScope()
{
	check(ActiveScopeSlot() == this);
	ActiveScopeSlot() = Previous;
}

bool FVoxelGenerationPlanningReadScope::IsActive()
{
	return ActiveScopeSlot() != nullptr;
}

void FVoxelGenerationPlanningReadScope::Missing(const EVoxelGenerationPlanKind InKind, const FIntVector& InCoordinate)
{
	// 子采样缺失同样属于调用者的实际读集，不能被嵌套范围隐藏。
	for (auto* Scope = ActiveScopeSlot(); Scope; Scope = Scope->Previous)
	{
		Scope->bMissing = true;
		if (Scope->Observer) (*Scope->Observer)(InKind, InCoordinate);
	}
}

bool FVoxelGenerationPlanningReadScope::CanPublish()
{
	for (auto* Scope = ActiveScopeSlot(); Scope; Scope = Scope->Previous)
		if (Scope->bMissing || (Scope->Cancel && Scope->Cancel->Load())) return false;
	return true;
}

FString FVoxelGenerationPlanningReadScope::PublishBlockError()
{
	for (auto* Scope = ActiveScopeSlot(); Scope; Scope = Scope->Previous)
		if (Scope->Cancel && Scope->Cancel->Load()) return TEXT("Canceled");
	return TEXT("DependencyNotReady: 项目事实实际自然采样");
}

const FVoxelTaskStamp* FVoxelGenerationPlanningReadScope::GetTaskStamp()
{
	for (auto* Scope = ActiveScopeSlot(); Scope; Scope = Scope->Previous)
		if (Scope->Stamp) return Scope->Stamp;
	return nullptr;
}

const FVoxelGenerationConsumerKey* FVoxelGenerationPlanningReadScope::GetConsumerKey()
{
	for (auto* Scope = ActiveScopeSlot(); Scope; Scope = Scope->Previous)
		if (Scope->Consumer) return Scope->Consumer;
	return nullptr;
}
