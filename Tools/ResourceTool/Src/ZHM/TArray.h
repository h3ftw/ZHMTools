#pragma once

#include <cstdint>
#include <cstdlib>

#include <ZHM/ZHMSerializer.h>
#include <Util/PortableIntrinsics.h>

template <class T>
class TIterator
{
protected:
	TIterator(T* p_Current) : m_pCurrent(p_Current) {}
	
public:
	T* m_pCurrent;
};

template <class T>
class TArray
{
public:
	TArray() :
		m_pBegin(nullptr),
		m_pEnd(nullptr),
		m_pAllocationEnd(nullptr)
	{
	}

	void push_back(const T& p_Value)
	{
		// TODO: Inline support.
		size_t s_Size = size();
		resize(s_Size + 1);
		m_pBegin[s_Size] = p_Value;
	}

	void resize(size_t p_Size)
	{
		// TODO: Inline support.
		if (capacity() == p_Size)
			return;

		if (m_pBegin == nullptr)
		{
			m_pBegin = reinterpret_cast<T*>(c_aligned_alloc(c_get_aligned(sizeof(T), alignof(T)) * p_Size, alignof(T)));
			m_pEnd = m_pBegin + p_Size;
			m_pAllocationEnd = m_pEnd;
			return;
		}

		T* s_NewMemory = reinterpret_cast<T*>(c_aligned_alloc(c_get_aligned(sizeof(T), alignof(T)) * p_Size, alignof(T)));
		memcpy(s_NewMemory, m_pBegin, c_get_aligned(sizeof(T), alignof(T)) * size());
		c_aligned_free(m_pBegin);

		m_pBegin = s_NewMemory;
		m_pEnd = m_pBegin + p_Size;
		m_pAllocationEnd = m_pEnd;
	}

	void clear()
	{
		if (m_pBegin == nullptr)
			return;

		// TODO: Free individual items.

		if (!fitsInline() || !hasInlineFlag())
			free(m_pBegin);
		
		m_pBegin = m_pEnd = m_pAllocationEnd = nullptr;
	}

	inline size_t size() const
	{
		if (fitsInline() && hasInlineFlag())
			return m_nInlineCount;
		
		return (reinterpret_cast<uintptr_t>(m_pEnd) - reinterpret_cast<uintptr_t>(m_pBegin)) / c_get_aligned(sizeof(T), alignof(T));
	}

	inline size_t capacity() const
	{
		if (fitsInline() && hasInlineFlag())
			return m_nInlineCapacity;

		return (reinterpret_cast<uintptr_t>(m_pAllocationEnd) - reinterpret_cast<uintptr_t>(m_pBegin)) / c_get_aligned(sizeof(T), alignof(T));
	}

	inline T& operator[](size_t p_Index) const
	{
		return begin()[p_Index];
	}

	inline T* begin()
	{
		if (fitsInline() && hasInlineFlag())
			return reinterpret_cast<T*>(&m_pBegin);
		
		return m_pBegin;
	}

	inline T* end()
	{
		if (fitsInline() && hasInlineFlag())
			return begin() + m_nInlineCount;

		return m_pEnd;
	}

	inline T* begin() const
	{
		if (fitsInline() && hasInlineFlag())
			return (T*) (&m_pBegin);

		return m_pBegin;
	}

	inline T* end() const
	{
		if (fitsInline() && hasInlineFlag())
			return begin() + m_nInlineCount;

		return m_pEnd;
	}

	inline T* find(const T& p_Value) const
	{
		T* s_Current = begin();

		while (s_Current != end())
		{
			if (*s_Current == p_Value)
				return s_Current;

			++s_Current;
		}

		return m_pEnd;
	}

	bool fitsInline() const
	{
		return sizeof(T) <= sizeof(T*) * 2;
	}

	bool hasInlineFlag() const
	{
		return (m_nFlags >> 62) & 1;
	}

	static void Serialize(void* p_Object, ZHMSerializer& p_Serializer, uintptr_t p_OwnOffset)
	{
		auto* s_Object = reinterpret_cast<TArray<T>*>(p_Object);
		
		if (s_Object->hasInlineFlag())
			throw std::exception("cannot serialize inline arrays");

		if (s_Object->size() == 0)
		{
			p_Serializer.PatchNullPtr(p_OwnOffset + offsetof(TArray<T>, m_pBegin));
			p_Serializer.PatchNullPtr(p_OwnOffset + offsetof(TArray<T>, m_pEnd));
			p_Serializer.PatchNullPtr(p_OwnOffset + offsetof(TArray<T>, m_pAllocationEnd));
		}
		else
		{
			auto s_ElementsPtr = p_Serializer.WriteMemory(s_Object->m_pBegin, c_get_aligned(sizeof(T), alignof(T)) * s_Object->size());

			for (size_t i = 0; i < s_Object->size(); ++i)
			{
				auto& s_Item = s_Object->begin()[i];

				if constexpr(!std::is_fundamental_v<T> && !std::is_enum_v<T>)
				{
					uintptr_t s_Offset = s_ElementsPtr + c_get_aligned(sizeof(T), alignof(T)) * i;
					T::Serialize(&s_Item, p_Serializer, s_Offset);
				}
			}

			p_Serializer.PatchPtr(p_OwnOffset + offsetof(TArray<T>, m_pBegin), s_ElementsPtr);
			p_Serializer.PatchPtr(p_OwnOffset + offsetof(TArray<T>, m_pEnd), s_ElementsPtr + c_get_aligned(sizeof(T), alignof(T)) * s_Object->size());
			p_Serializer.PatchPtr(p_OwnOffset + offsetof(TArray<T>, m_pAllocationEnd), s_ElementsPtr + c_get_aligned(sizeof(T), alignof(T)) * s_Object->size());
		}
	}

public:
	T* m_pBegin;
	T* m_pEnd;

	union
	{
		T* m_pAllocationEnd;
		int64_t m_nFlags;

		struct
		{
			uint8_t m_nInlineCount;
			uint8_t m_nInlineCapacity;
		};
	};
};

template<typename T>
class TFixedArray
{
public:
	inline size_t size() const
	{
		return (reinterpret_cast<uintptr_t>(m_pEnd) - reinterpret_cast<uintptr_t>(m_pBegin)) / sizeof(uintptr_t);
	}

	inline size_t capacity() const
	{
		return (reinterpret_cast<uintptr_t>(m_pEnd) - reinterpret_cast<uintptr_t>(m_pBegin)) / sizeof(uintptr_t);
	}

	inline T& operator[](size_t p_Index) const
	{
		return m_pBegin[p_Index];
	}

	inline T* begin()
	{
		return m_pBegin;
	}

	inline T* end()
	{
		return m_pEnd;
	}

	inline T* begin() const
	{
		return m_pBegin;
	}

	inline T* end() const
	{
		return m_pEnd;
	}

	inline T* find(const T& p_Value) const
	{
		T* s_Current = m_pBegin;

		while (s_Current != m_pEnd)
		{
			if (*s_Current == p_Value)
				return s_Current;

			++s_Current;
		}

		return m_pEnd;
	}

public:
	T* m_pBegin;
	T* m_pEnd;
};