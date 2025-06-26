#include "ChunkPool.hpp"


ChunkPool::ChunkPool(World &world, TextureManager &textManager, int nbChunks)
: _world(world), _textureManager(textManager) {
	if (nbChunks > 180000)
		nbChunks = 180000;
	for (int i = 0; i < nbChunks; i++) {
		Chunk *chunk = new Chunk(world, textManager);
		_pool.push_back(chunk);
	}
}
ChunkPool::~ChunkPool() {
	for (size_t i = 0; i < _pool.size(); i++)
		delete _pool[i];
}
Chunk* ChunkPool::acquire() {
	std::lock_guard<std::mutex> lock(_mutex);
	if (!_pool.empty()) {
		Chunk* chunk = _pool.back();
		_pool.pop_back();
		return chunk;
	}
	return new Chunk(_world, _textureManager);
}
void ChunkPool::release(Chunk* chunk) {
	std::lock_guard<std::mutex> lock(_mutex);
	chunk->reset();
	_pool.push_back(chunk);
}
