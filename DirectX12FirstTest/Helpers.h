#pragma once
#ifndef __HELPERS_H__
#define __HELPERS_H__
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <exception>

inline void ThrowIfFailed(HRESULT hr) {
	if (FAILED(hr)) {
		throw std::exception();
	}
}

#endif
