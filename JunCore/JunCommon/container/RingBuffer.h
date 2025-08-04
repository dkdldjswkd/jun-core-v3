#pragma once
#include <memory>
#include <BaseTsd.h>

#define MASKING_8BIT(n)		(0x00ff	& (n)) // 256
#define MASKING_9BIT(n)		(0x01ff & (n)) // 512
#define MASKING_10BIT(n)	(0x03ff & (n)) // 1024
#define MASKING_11BIT(n)	(0x07ff & (n)) // 2048
#define MASKING_12BIT(n)	(0x0fff & (n)) // 4096
#define MASKING_13BIT(n)	(0x1fff & (n)) // 8192

// 실제 크기 설정
#define MASKING_BIT(n)		MASKING_13BIT(n)
#define BUF_SIZE			8192

class RingBuffer {
public:
	RingBuffer();
	~RingBuffer();

private:
	char* begin;
	char* end;

	char* front;
	char* rear;

	char* readPos;
	char* writePos;

private:
	inline int MustEnqueue(const void* src, size_t size);
	inline int MustDequeue(void* dst, size_t size);

	inline void SetReadPos() { readPos = begin + MASKING_BIT((ULONG_PTR)front + 1); }
	inline void SetWritePos() { writePos = begin + MASKING_BIT((ULONG_PTR)rear + 1); }

public:
	inline void MoveFront(int size) { front = begin + MASKING_BIT((ULONG_PTR)front + size); SetReadPos();  }
	inline void MoveRear(int size) { rear = begin + MASKING_BIT((ULONG_PTR)rear + size);    SetWritePos(); }

	inline bool Empty() const { return front == rear; }
	inline bool Full() const { return front == writePos; }
	inline void Clear();

	inline int DirectEnqueueSize() const;
	inline int DirectDequeueSize() const;
	inline int RemainEnqueueSize() const;
	inline int RemainDequeueSize() const;

	// inline ���� �ȵ�
	inline int GetFreeSize() const;
	inline int GetUseSize() const;

	inline char* GetReadPos() const { return readPos; }
	inline char* GetWritePos()  const { return writePos; }
	inline char* GetBeginPos() const { return begin; }

	int Enqueue(const void* src, size_t size);
	int Dequeue(void* dst, size_t size);
	int Peek(void* dst, size_t size) const;
};

inline int RingBuffer::MustEnqueue(const void* src, size_t size) {
	memmove(writePos, src, size);
	MoveRear(static_cast<int>(size));
	return static_cast<int>(size);
}

inline int RingBuffer::MustDequeue(void* dst, size_t size) {
	memmove(dst, readPos, size);
	MoveFront(static_cast<int>(size));
	return static_cast<int>(size);
}

inline void RingBuffer::Clear(){
	front = begin;
	rear = begin;

	writePos = front + 1;
	readPos = rear + 1;
}

///////////////////////////////////////////////////////////////////////
// Enqueue, Dequeue Size 에서 첫번째 바이트는 사용해야 함 
// 
// ex. 첫번째 바이트 영역이(wp, rp) 모두 앞에 있다면, end 영역 바로 뒤
// 다시 begin 영역 첫번째 바이트(wp, rp)까지 사용해야함
///////////////////////////////////////////////////////////////////////

inline int RingBuffer::DirectEnqueueSize() const {
	if (front < writePos) return static_cast<int>(end - writePos); // 첫번째 바이트 (front) 이후
	else return static_cast<int>(front - writePos);
}

inline int RingBuffer::RemainEnqueueSize() const {
	if (writePos <= front) return 0; // 첫번째 바이트 없음
	else return static_cast<int>(front - begin);
}

inline int RingBuffer::DirectDequeueSize() const {
	if (writePos < readPos) return static_cast<int>(end - readPos); // 첫번째 바이트  (write pos) 이후
	else return static_cast<int>(writePos - readPos);
}

inline int RingBuffer::RemainDequeueSize() const {
	if (readPos <= writePos) return 0; // 첫번째 바이트 없음
	else return static_cast<int>(writePos - begin);
}

int RingBuffer::GetFreeSize() const {
	return  DirectEnqueueSize() + RemainEnqueueSize();
}
inline int RingBuffer::GetUseSize() const {
	return  DirectDequeueSize() + RemainDequeueSize();
};

inline static int MustPeek(void* dst, const void* src, size_t size) {
	memmove(dst, src, size);
	return static_cast<int>(size);
}