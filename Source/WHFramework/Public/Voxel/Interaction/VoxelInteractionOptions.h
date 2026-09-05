#pragma once

#include "Common/Interaction/InteractionAction.h"
#include "Common/Interaction/InteractionCondition.h"
#include "Common/Interaction/InteractionOption.h"
#include "VoxelInteractionOptions.generated.h"

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionCondition_VoxelInteract : public UInteractionCondition
{
	GENERATED_BODY()

public:
	virtual bool Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionAction_VoxelInteract : public UInteractionAction
{
	GENERATED_BODY()

public:
	virtual bool Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionOption_VoxelInteract : public UInteractionOption
{
	GENERATED_BODY()

public:
	UInteractionOption_VoxelInteract();
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionCondition_VoxelUnInteract : public UInteractionCondition
{
	GENERATED_BODY()

public:
	virtual bool Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionAction_VoxelUnInteract : public UInteractionAction
{
	GENERATED_BODY()

public:
	virtual bool Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionOption_VoxelUnInteract : public UInteractionOption
{
	GENERATED_BODY()

public:
	UInteractionOption_VoxelUnInteract();
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionCondition_VoxelOpen : public UInteractionCondition
{
	GENERATED_BODY()

public:
	virtual bool Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionAction_VoxelOpen : public UInteractionAction
{
	GENERATED_BODY()

public:
	virtual bool Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionOption_VoxelOpen : public UInteractionOption
{
	GENERATED_BODY()

public:
	UInteractionOption_VoxelOpen();
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionCondition_VoxelClose : public UInteractionCondition
{
	GENERATED_BODY()

public:
	virtual bool Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionAction_VoxelClose : public UInteractionAction
{
	GENERATED_BODY()

public:
	virtual bool Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionOption_VoxelClose : public UInteractionOption
{
	GENERATED_BODY()

public:
	UInteractionOption_VoxelClose();
};
