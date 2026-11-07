#pragma once

#include "CoreMinimal.h"

/** Process-local native type identity; do not persist or send its numeric value. */
struct WHFRAMEWORKCORE_API FUniqueType
{
public:
	using FUniqueID = uint32;

	FUniqueType(const FUniqueType* Parent = nullptr)
		: ID(++NextUniqueID)
		, ParentType(Parent)
	{
		UE_CLOG(ID == 0, LogTemp, Fatal, TEXT("Native type identity space exhausted"));
	}

	bool operator==(const FUniqueType& Other) const
	{
		return ID == Other.ID;
	}

	bool IsA(const FUniqueType& Other) const
	{
		return (ID == Other.ID) || (ParentType && ParentType->IsA(Other));
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("UniqueType_%u"), ID);
	}

	friend uint32 GetTypeHash(const FUniqueType& Type)
	{
		return GetTypeHash(Type.ID);
	}

private:
	static TAtomic<FUniqueID> NextUniqueID;
	FUniqueID ID;
	const FUniqueType* ParentType;
};

/*
 * FUniqueStruct
 */
struct WHFRAMEWORKCORE_API FUniqueStruct
{
public:
	FUniqueStruct()
		: StructType(FUniqueType())
	{
	}

	FUniqueStruct(FUniqueType InType)
		: StructType(InType)
	{
	}

	virtual ~FUniqueStruct() {}

	static const FUniqueType Type;

public:
	template <typename T>
	T* Cast()
	{
		return StructType.IsA(T::Type) ? static_cast<T*>(this) : nullptr;
	}
	template <typename T>
	const T* Cast() const
	{
		return StructType.IsA(T::Type) ? static_cast<const T*>(this) : nullptr;
	}
	template <typename T>
	T& CastRef()
	{
		T* Value = Cast<T>();
		UE_CLOG(!Value, LogTemp, Fatal, TEXT("Invalid native struct type cast"));
		return *Value;
	}
	template <typename T>
	const T& CastRef() const
	{
		const T* Value = Cast<T>();
		UE_CLOG(!Value, LogTemp, Fatal, TEXT("Invalid native struct type cast"));
		return *Value;
	}
	template <typename T>
	bool IsA() const
	{
		return StructType.IsA(T::Type);
	}

protected:
	FUniqueType StructType;

public:
	FUniqueType GetStructType() const { return StructType; }
};

/*
 * FUniqueClass
 */
class WHFRAMEWORKCORE_API FUniqueClass
{
public:
	FUniqueClass()
		: ClassType(FUniqueType())
	{
	}

	FUniqueClass(FUniqueType InType)
		: ClassType(InType)
	{
	}

	virtual ~FUniqueClass() {}

	static const FUniqueType Type;

public:
	template <typename T>
	T* Cast()
	{
		return ClassType.IsA(T::Type) ? static_cast<T*>(this) : nullptr;
	}
	template <typename T>
	const T* Cast() const
	{
		return ClassType.IsA(T::Type) ? static_cast<const T*>(this) : nullptr;
	}
	template <typename T>
	T& CastRef()
	{
		T* Value = Cast<T>();
		UE_CLOG(!Value, LogTemp, Fatal, TEXT("Invalid native class type cast"));
		return *Value;
	}
	template <typename T>
	const T& CastRef() const
	{
		const T* Value = Cast<T>();
		UE_CLOG(!Value, LogTemp, Fatal, TEXT("Invalid native class type cast"));
		return *Value;
	}
	template <typename T>
	bool IsA() const
	{
		return ClassType.IsA(T::Type);
	}

protected:
	FUniqueType ClassType;

public:
	FUniqueType GetClassType() const { return ClassType; }
};
