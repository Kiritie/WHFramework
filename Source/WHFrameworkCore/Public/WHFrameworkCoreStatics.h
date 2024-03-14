#pragma once

class FWHFrameworkCoreStatics
{
public:
	//////////////////////////////////////////////////////////////////////////
	// Array
	template<class T>
	static void SortStringElementArray(TArray<T>& InArray, const TFunction<FString(const T&)>& InElementFunc, bool bAscending = true)
	{
		for(int32 i = 0; i < InArray.Num(); i++)
		{
			for(int32 j = i; j < InArray.Num(); j++)
			{
				const FString Str1 = InElementFunc(InArray[i]);
				const FString Str2 = InElementFunc(InArray[j]);
				int32 Num1 = 0;
				int32 Num2 = 0;
				for(int32 k = 0; k < (Str1.Len() < Str2.Len() ? Str1.Len() : Str2.Len()); k++)
				{
					Num1 = Str1.GetCharArray()[k];
					Num2 = Str2.GetCharArray()[k];
					if(Num1 != Num2)
					{
						break;
					}
				}
				if(bAscending ? Num1 < Num2 : Num1 > Num2)
				{
					InArray.Swap(i, j);
				}
			}
		}
	}
};
