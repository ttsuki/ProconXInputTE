#pragma once

struct IUncopyable
{
	IUncopyable() = default;
	IUncopyable(const IUncopyable& other) = delete;
	IUncopyable& operator=(const IUncopyable& other) = delete;
};
