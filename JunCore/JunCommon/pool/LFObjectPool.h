#pragma once
#include "../core/base.h"
#include <Windows.h>
#include <stdarg.h>
#include <memory>
#include <BaseTsd.h>

template <typename T>
class LFObjectPool {
private:
	struct Node {
	public:
		Node(ULONG_PTR integrity) : integrity(integrity), under_guard(kMemGuard), object(), over_guard(kMemGuard), next(nullptr) {};

	public:
		const ULONG_PTR integrity;
		const size_t under_guard;
		T object;
		const size_t over_guard;
		Node* next = nullptr;
	};

public:
	LFObjectPool(int node_num = 0, bool use_ctor = false);
	~LFObjectPool();

private:
	const ULONG_PTR integrity_;
	alignas(64) ULONG_PTR top_stamp_;
	int object_offset_;

	// Opt
	bool use_ctor_;

	// Count
	alignas(64) int capacity__;
	alignas(64) int use_count_;

public:
	T* Alloc();
	void Free(T* object);

	// Opt
	void SetUseCtor(bool is_use) { use_ctor_ = is_use; }

	// Getter
	inline int GetCapacityCount() const { return capacity__; }
	inline int GetUseCount() const { return use_count_; }
};

//------------------------------
// ObjectPool
//------------------------------

template<typename T>
LFObjectPool<T>::LFObjectPool(int node_num, bool use_ctor) : integrity_((ULONG_PTR)this), use_ctor_(use_ctor), top_stamp_(NULL), capacity__(node_num), use_count_(0) {
	// Stamp 사용 가능 주소 확인
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	if (0 != ((ULONG_PTR)sysInfo.lpMaximumApplicationAddress & kStampMask))
		throw std::exception("STAMP_N/A");

	// object_offset 초기화
	Node tmpNode((ULONG_PTR)this);
	object_offset_ = static_cast<int>((ULONG_PTR)(&tmpNode.object) - (ULONG_PTR)&tmpNode);

	// 노드들을 미리 생성 및 연결
	for (int i = 0; i < node_num; i++) {
		Node* newNode = new Node((ULONG_PTR)this);
		newNode->next = (Node*)top_stamp_;
		top_stamp_ = (ULONG_PTR)newNode;
	}
}

template<typename T>
LFObjectPool<T>::~LFObjectPool() {
	Node* top = (Node*)(top_stamp_ & kUseBitMask);

	for (; top != nullptr;) {
		Node* deleteNode = top;
		top = top->next;
		delete deleteNode;
	}
}

// 메모리 할당은 POP과 같음
template<typename T>
T* LFObjectPool<T>::Alloc() {
	for (;;) {
		DWORD64 copyTopStamp = top_stamp_;
		Node* topClean = (Node*)(copyTopStamp & kUseBitMask);

		// Not empty!!
		if (topClean) {
			// Stamp 증가 후 newTopStamp 생성
			DWORD64 nextStamp = (copyTopStamp + kStampCount) & kStampMask;
			DWORD64 newTopStamp = nextStamp | (DWORD64)topClean->next;

			// 도중에 변화가 있었다면 다시시도
			if (copyTopStamp != InterlockedCompareExchange64((LONG64*)&top_stamp_, (LONG64)newTopStamp, (LONG64)copyTopStamp))
				continue;

			if (use_ctor_) {
				new (&topClean->object) T;
			}

			InterlockedIncrement((LONG*)&use_count_);
			return &topClean->object;
		}
		// empty!!
		else {
			InterlockedIncrement((LONG*)&capacity__);
			InterlockedIncrement((LONG*)&use_count_);

			// Node 와 Object 생성하여 ret
			return &(new Node((ULONG_PTR)this))->object;
		}
	}
}

// 메모리 반납은 PUSH와 같음
template<typename T>
void LFObjectPool<T>::Free(T* object) {
	// 오브젝트 주소 변환
	Node* pushNode = (Node*)((char*)object - object_offset_);

	if (integrity_ != pushNode->integrity)
		throw std::exception("ERROR_INTEGRITY");
	if (kMemGuard != pushNode->over_guard)
		throw std::exception("ERROR_INVAID_OVER");
	if (kMemGuard != pushNode->under_guard)
		throw std::exception("ERROR_INVAID_UNDER");

	if (use_ctor_) {
		object->~T();
	}

	for (;;) {
		DWORD64 copyTopStamp = top_stamp_;

		// top에 연결함
		pushNode->next = (Node*)(copyTopStamp & kUseBitMask);

		// Stamp 증가 후 newTopStamp 생성
		DWORD64 nextStamp = (copyTopStamp + kStampCount) & kStampMask;
		DWORD64 newTopStamp = nextStamp | (DWORD64)pushNode;

		// 도중에 변화가 있었다면 다시시도
		if (copyTopStamp != InterlockedCompareExchange64((LONG64*)&top_stamp_, (LONG64)newTopStamp, (LONG64)copyTopStamp))
			continue;

		InterlockedDecrement((LONG*)&use_count_);
		return;
	}
}