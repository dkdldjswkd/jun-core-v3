#pragma once
#include <Windows.h>
#include "../pool/LFObjectPool.h"
#include "../core/base.h"

template <typename T>
struct LFStack {
public:
	struct Node {
	public:
		Node();
		Node(const T& data);
		~Node();

	public:
		T data;
		Node* next;

	public:
		void Set();
	};

public:
	LFStack();
	~LFStack();

private:
	LFObjectPool<Node> nodePool;

private:
	alignas(64) DWORD64 topStamp = NULL;
	alignas(64) int nodeCount = 0;

public:
	void Push(T data);
	bool Pop(T* dst);
	int GetUseCount();
};

//------------------------------
// LFStack
//------------------------------

template <typename T>
LFStack<T>::LFStack() : nodePool(0, true) {
}

template <typename T>
LFStack<T>::~LFStack() {
	Node* top = (Node*)(topStamp & kUseBitMask);

	for (; top != nullptr;) {
		Node* delete_node = top;
		top = top->next;
		nodePool.Free(delete_node);
	}
}

template <typename T>
void LFStack<T>::Push(T data) {
	// Create Node
	Node* pushNode = nodePool.Alloc();
	pushNode->Set();
	pushNode->data = data;

	for (;;) {
		DWORD64 copyTopStamp = topStamp;

		// top�� �̾���
		pushNode->next = (Node*)(copyTopStamp & kUseBitMask);

		// Stamp ���� �� newTopStamp ����
		DWORD64 nextStamp = (copyTopStamp + kStampCount) & kStampMask;
		DWORD64 newTopStamp = nextStamp | (DWORD64)pushNode;

		// ���ÿ� ��ȭ�� �־��ٸ� �ٽýõ�
		if (copyTopStamp != InterlockedCompareExchange64((LONG64*)&topStamp, (LONG64)newTopStamp, (LONG64)copyTopStamp))
			continue;

		InterlockedIncrement((LONG*)&nodeCount);
		return;
	}
}

template <typename T>
bool LFStack<T>::Pop(T* dst) {
	for (;;) {
		DWORD64 copyTopStamp = topStamp;
		Node* topClean = (Node*)(copyTopStamp & kUseBitMask);

		// Not empty!!
		if (topClean) {
			// Stamp ���� �� newTopStamp ����
			DWORD64 nextStamp = (copyTopStamp + kStampCount) & kStampMask;
			DWORD64 newTopStamp = nextStamp | (DWORD64)topClean->next;

			// ���ÿ� ��ȭ�� �־��ٸ� �ٽýõ�
			if (copyTopStamp != InterlockedCompareExchange64((LONG64*)&topStamp, (LONG64)newTopStamp, (LONG64)copyTopStamp))
				continue;

			InterlockedDecrement((LONG*)&nodeCount);
			*dst = topClean->data;
			nodePool.Free(topClean);
			return true;
		}
		// empty!!
		else {
			return false;
		}
	}
}

template <typename T>
int LFStack<T>::GetUseCount() {
	return nodeCount;
}

//------------------------------
// Node
//------------------------------

template <typename T>
LFStack<T>::Node::Node() : next(nullptr) {
}

template <typename T>
LFStack<T>::Node::Node(const T& data) : data(data), next(nullptr) {
}

template <typename T>
LFStack<T>::Node::~Node() {
}

template<typename T>
void LFStack<T>::Node::Set() {
	next = nullptr;
}