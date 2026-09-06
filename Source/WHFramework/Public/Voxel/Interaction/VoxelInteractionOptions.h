#pragma once

#include "Common/Interaction/InteractionActionBase.h"
#include "Common/Interaction/InteractionConditionBase.h"
#include "Common/Interaction/InteractionOptionBase.h"
#include "VoxelInteractionOptions.generated.h"

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionCondition_VoxelInteract : public UInteractionConditionBase
{
	GENERATED_BODY()

public:
	virtual bool Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionAction_VoxelInteract : public UInteractionActionBase
{
	GENERATED_BODY()

public:
	virtual bool Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionOption_VoxelInteract : public UInteractionOptionBase
{
	GENERATED_BODY()

public:
	UInteractionOption_VoxelInteract();
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionCondition_VoxelUnInteract : public UInteractionConditionBase
{
	GENERATED_BODY()

public:
	virtual bool Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionAction_VoxelUnInteract : public UInteractionActionBase
{
	GENERATED_BODY()

public:
	virtual bool Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionOption_VoxelUnInteract : public UInteractionOptionBase
{
	GENERATED_BODY()

public:
	UInteractionOption_VoxelUnInteract();
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionCondition_VoxelOpen : public UInteractionConditionBase
{
	GENERATED_BODY()

public:
	virtual bool Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionAction_VoxelOpen : public UInteractionActionBase
{
	GENERATED_BODY()

public:
	virtual bool Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
	virtual void Finish_Implementation(const FInteractionContext& InContext, EInteractionActionState InState) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionOption_VoxelOpen : public UInteractionOptionBase
{
	GENERATED_BODY()

public:
	UInteractionOption_VoxelOpen();
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionCondition_VoxelClose : public UInteractionConditionBase
{
	GENERATED_BODY()

public:
	virtual bool Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionAction_VoxelClose : public UInteractionActionBase
{
	GENERATED_BODY()

public:
	virtual bool Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionOption_VoxelClose : public UInteractionOptionBase
{
	GENERATED_BODY()

public:
	UInteractionOption_VoxelClose();
};
