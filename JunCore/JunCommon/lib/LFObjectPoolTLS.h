#pragma once
#include "LFObjectPool.h"

#define CHUNCK_SIZE 500
#define USE_COUNT	0		// 0 : ûũ ���� ī��Ʈ, 1 : ���� ���� ī��Ʈ 

template <typename T>
class LFObjectPoolTLS {
public:
	LFObjectPoolTLS(bool use_ctor = false) : use_ctor_(use_ctor), tls_index_(TlsAlloc()) {};
	~LFObjectPoolTLS() {}

private:
	struct Chunk;
	struct ChunkData {
	public:
		Chunk* my_chunk_;
		T object;

	public:
		void Free() {
			my_chunk_->Free(&object);
		}
	};

	struct Chunk {
	public:
		Chunk() {}
		~Chunk() {}

	private:
		LFObjectPool<Chunk>* chunk_pool_;
		bool use_ctor_;
		int tls_index_;

	private:
		ChunkData chunk_data_array_[CHUNCK_SIZE];
		int alloc_index_;
		alignas(64) int free_index_;

	public:
		void Set(LFObjectPool<Chunk>* chunk_pool, bool use_ctor, int tls_index) {
			this->chunk_pool_ = chunk_pool;
			this->use_ctor_ = use_ctor;
			this->tls_index_ = tls_index;

			alloc_index_ = 0;
			free_index_ = 0;
			for (int i = 0; i < CHUNCK_SIZE; i++) {
				chunk_data_array_[i].my_chunk_ = this;
			}
		}

		T* Alloc() {
			T* object = &(chunk_data_array_[alloc_index_++].object);
			if (use_ctor_) new (object) T;

			if (CHUNCK_SIZE == alloc_index_) {
				Chunk* chunk = chunk_pool_->Alloc();
				chunk->Set(chunk_pool_, use_ctor_, tls_index_);
				TlsSetValue(tls_index_, (LPVOID)chunk);
			}
			return object;
		}

		void Free(T* object) {
			if (use_ctor_) object->~T();

			if (CHUNCK_SIZE == InterlockedIncrement((DWORD*)&free_index_)) {
				chunk_pool_->Free(this);
			}
		}
	};

private:
	LFObjectPool<Chunk> chunk_pool_;
	const int tls_index_;
	const bool use_ctor_;
	int count_ = 0;

public:
	int GetChunkCapacity() {
		return chunk_pool_.GetCapacityCount();
	}

	int GetChunkUseCount() {
		return chunk_pool_.GetUseCount();
	}

	int GetUseCount() {
#if USE_COUNT
		return count_;
#else
		return GetChunkUseCount() * CHUNCK_SIZE;
#endif
	}

	T* Alloc() {
#if USE_COUNT
		InterlockedIncrement((LONG*)&count_);
#endif

		Chunk* chunk = (Chunk*)TlsGetValue(tls_index_);
		if (nullptr == chunk) {
			chunk = chunk_pool_.Alloc();
			chunk->Set(&chunk_pool_, use_ctor_, tls_index_);
			TlsSetValue(tls_index_, chunk);
		}

		return chunk->Alloc();
	}

	void Free(T* object) {
#if USE_COUNT
		InterlockedDecrement((LONG*)&count_);
#endif

		ChunkData* chunk_data = (ChunkData*)((char*)object - sizeof(Chunk*));
		chunk_data->Free();
	}
};
