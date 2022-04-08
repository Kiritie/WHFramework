#pragma once

#include "AssetModuleTypes.generated.h"
class UItemDataBase;
/**
 * ???????
 */
UENUM(BlueprintType)
enum class EItemType : uint8
{
	// ??
	None,
	// ????
	Voxel,
	// ????
	Prop,
	// ???
	Equip,
	// ????
	Skill,
	// ????
	Vitality,
	// ????
	Character
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FItem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FPrimaryAssetId ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Count;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level;

	static FItem Empty;

public:
	FORCEINLINE FItem()
	{
		Count = 0;
		Level = 0;
	}
		
	FORCEINLINE FItem(const FPrimaryAssetId& InID, int32 InCount, int32 InLevel = 0)
	{
		ID = InID;
		Count = InCount;
		Level = InLevel;
	}
	
	FORCEINLINE FItem(const FItem& InItem, int InCount = -1)
	{
		ID = InItem.ID;
		Count = InCount == -1 ? InItem.Count : InCount;
		Level = InItem.Level;
	}

	virtual ~FItem() = default;

	template<class T>
	T* GetData() const
	{
		return Cast<T>(GetData());
	}

	UItemDataBase* GetData() const;

	FORCEINLINE virtual bool IsValid() const
	{
		return ID.IsValid();
	}

	FORCEINLINE bool EqualType(FItem InItem) const
	{
		return InItem.IsValid() && InItem.ID == ID && InItem.Level == Level;
	}

	FORCEINLINE friend bool operator==(const FItem& A, const FItem& B)
	{
		return (A.ID == B.ID) && (A.Count == B.Count) && (A.Level == B.Level);
	}

	FORCEINLINE friend bool operator!=(const FItem& A, const FItem& B)
	{
		return (A.ID != B.ID) || (A.Count != B.Count) || (A.Level != B.Level);
	}

	FORCEINLINE friend FItem operator+(FItem& A, FItem& B)
	{
		if(A.EqualType(B))
		{
			A.Count += B.Count;
		}
		return A;
	}

	FORCEINLINE friend FItem operator-(FItem& A, FItem& B)
	{
		if(A.EqualType(B))
		{
			A.Count -= B.Count;
		}
		return A;
	}

	FORCEINLINE friend FItem& operator+=(FItem& A, FItem& B)
	{
		if(A.EqualType(B))
		{
			A.Count += B.Count;
		}
		return A;
	}

	FORCEINLINE friend FItem& operator-=(FItem& A, FItem& B)
	{
		if(A.EqualType(B))
		{
			A.Count -= B.Count;
		}
		return A;
	}
};
