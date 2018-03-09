#pragma once

#include "Common/DeviceResources.h"

enum GLenum {
	GL_TRIANGLES,

	GL_UNSIGNED_SHORT,

	GL_ARRAY_BUFFER,
	GL_ELEMENT_ARRAY_BUFFER,

	GL_STATIC_DRAW
};

typedef int GLsizei;
typedef unsigned int GLuint;
typedef void GLvoid;
typedef int* GLsizeiptr;
typedef bool GLboolean;

void lpglInit(const std::shared_ptr<DX::DeviceResources>& deviceResources);

DX::DeviceResources& lpglGetDeviceResources();

void __lpglDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
	const void * indices, GLsizei primcount);

void __lpglLoadIdentity();

void __lpglTranslatef(float x, float y, float z);

void __lpglGenBuffers(GLsizei n, GLuint * buffers);

void __lpglBindBuffer(GLenum target, GLuint buffer);

void __lpglBufferData(GLenum target, GLsizei size, const GLvoid* data, GLenum usage);

#define glDrawElementsInstanced(mode, count, type, indices, primcount) \
__lpglDrawElementsInstanced(mode, count, type, indices, primcount)

#define glLoadIdentity() \
__lpglLoadIdentity()

#define glTranslatef(x, y, z) \
__lpglTranslatef(x, y, z)

#define glGenBuffers(n, buffers) \
__lpglGenBuffers(n, buffers)

#define glBindBuffer(target, buffer) \
__lpglBindBuffer(target, buffer)

#define glBufferData(target, size, data, usage) \
__lpglBufferData(target, size, data, usage)