#pragma once
#include "trap.h"

struct Resolution {
	uintptr_t* path = reinterpret_cast<uint64_t*>(0x141148218);
	int width = 0;
	int height = 0;
	int offsetX = 0;
	int offsetY = 0;

	void clear() {
		*this = {};
	}

	void update() {
		width = *(int*)(*(path)+0x40);
		height = *(int*)(*(path)+0x44);
		offsetX = *(int*)(*(path)+0x48);
		offsetY = *(int*)(*(path)+0x4c);
	}
};

class TrapPSP : public Trap {
public:
	TrapPSP() : Trap("PSP") {}

	void config(const toml::table& settings);
	void save(toml::table& settings);
	void reset();
	void touch();
	void tick();
	void ImGuiConfig();
	void ImGuiStatus();

private:
	int height = 270;
	Resolution prevRes;
};
