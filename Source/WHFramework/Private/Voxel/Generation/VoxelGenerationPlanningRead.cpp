#include "Voxel/Generation/VoxelGenerationPlanningRead.h"

thread_local FVoxelGenerationConsumerScope* FVoxelGenerationConsumerScope::Active = nullptr;

FVoxelGenerationConsumerScope::FVoxelGenerationConsumerScope(const FVoxelGenerationConsumerKey& InConsumer)
	: Consumer(InConsumer)
	, Previous(Active)
{
	Active = this;
}

FVoxelGenerationConsumerScope::~FVoxelGenerationConsumerScope()
{
	check(Active == this);
	Active = Previous;
}

const FVoxelGenerationConsumerKey* FVoxelGenerationConsumerScope::GetConsumerKey()
{
	return Active && Active->Consumer.IsValid() ? &Active->Consumer : nullptr;
}

thread_local FVoxelGenerationPlanningReadScope* FVoxelGenerationPlanningReadScope::Active = nullptr;

FVoxelGenerationPlanningReadScope::FVoxelGenerationPlanningReadScope(FObserver InObserver,
	const FVoxelTaskStamp* InStamp, const TAtomic<bool>* InCancel, const FVoxelGenerationConsumerKey* InConsumer)
	: Observer(MoveTemp(InObserver))
	, Stamp(InStamp)
	, Cancel(InCancel)
	, Consumer(InConsumer)
	, Previous(Active)
{
	Active = this;
}

FVoxelGenerationPlanningReadScope::~FVoxelGenerationPlanningReadScope()
{
	check(Active == this);
	Active = Previous;
}

bool FVoxelGenerationPlanningReadScope::IsActive()
{
	return Active != nullptr;
}

void FVoxelGenerationPlanningReadScope::Missing(const EVoxelGenerationPlanKind InKind, const FIntVector& InCoordinate)
{
	// 子采样缺失同样属于调用者的实际读集，不能被嵌套范围隐藏。
	for (auto* Scope = Active; Scope; Scope = Scope->Previous)
	{
		Scope->bMissing = true;
		if (Scope->Observer) Scope->Observer(InKind, InCoordinate);
	}
}

bool FVoxelGenerationPlanningReadScope::CanPublish()
{
	for (auto* Scope = Active; Scope; Scope = Scope->Previous)
		if (Scope->bMissing || (Scope->Cancel && Scope->Cancel->Load())) return false;
	return true;
}

FString FVoxelGenerationPlanningReadScope::PublishBlockError()
{
	for (auto* Scope = Active; Scope; Scope = Scope->Previous)
		if (Scope->Cancel && Scope->Cancel->Load()) return TEXT("Canceled");
	return TEXT("DependencyNotReady: 项目事实实际自然采样");
}

const FVoxelTaskStamp* FVoxelGenerationPlanningReadScope::GetTaskStamp()
{
	for (auto* Scope = Active; Scope; Scope = Scope->Previous)
		if (Scope->Stamp) return Scope->Stamp;
	return nullptr;
}

const FVoxelGenerationConsumerKey* FVoxelGenerationPlanningReadScope::GetConsumerKey()
{
	for (auto* Scope = Active; Scope; Scope = Scope->Previous)
		if (Scope->Consumer) return Scope->Consumer;
	return nullptr;
}
