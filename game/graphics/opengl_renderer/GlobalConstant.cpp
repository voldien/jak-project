#include "game/graphics/opengl_renderer/GlobalConstant.h"

#include "common/util/math_util.h"

GlobalConstant::GlobalConstant() {
  GLint minMapBufferSize;
  glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &minMapBufferSize);
  this->globalSize = sizeof(GlobalConstant::ShaderGlobal);
  this->globalSize =
      align<size_t>(globalSize, (size_t)minMapBufferSize);
     this->uniformBufferSize = this->globalSize  * this->nrBuffers;

  glGenBuffers(1, (GLuint*)&this->bufferID);
  glBindBuffer(GL_UNIFORM_BUFFER, this->bufferID);
  glBufferData(GL_UNIFORM_BUFFER, this->uniformBufferSize, nullptr, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void GlobalConstant::bind() {
  glBindBufferRange(GL_UNIFORM_BUFFER, 12, bufferID, this->currentBuffer * this->globalSize,
                    this->globalSize);
  this->currentBuffer = (this->currentBuffer + 1) % nrBuffers;
}

void GlobalConstant::uploadConstant() {
  glBindBuffer(GL_UNIFORM_BUFFER, (GLuint)this->bufferID);
  void* uniformSSAOPointer = glMapBufferRange(
      GL_UNIFORM_BUFFER, this->currentBuffer * this->globalSize, this->globalSize,
      GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
  memcpy(uniformSSAOPointer, &this->Constants, sizeof(GlobalConstant::ShaderGlobal));
  glFlushMappedBufferRange(GL_UNIFORM_BUFFER, 0,
                           this->globalSize);
  glUnmapBuffer(GL_UNIFORM_BUFFER);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

unsigned int GlobalConstant::getBindingIndex() {
  return 16;
}

GlobalConstant::~GlobalConstant() {
  glDeleteBuffers(1, (GLuint*)&this->bufferID);
}