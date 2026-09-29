#pragma once
#include "trap.h"

namespace TrapPSP {
	struct Resolution {
		uintptr_t base() const {
			return *reinterpret_cast<uintptr_t*>(0x141148218);
		}

		int& width() { return *reinterpret_cast<int*>(base() + 0x40); }
		int& height() { return *reinterpret_cast<int*>(base() + 0x44); }
		int& offsetX() { return *reinterpret_cast<int*>(base() + 0x48); }
		int& offsetY() { return *reinterpret_cast<int*>(base() + 0x4C); }
	};

	class _TrapPSP : public Trap {
	public:
		void config(const toml::table& settings);
		void save(toml::table& settings);
		void resetRes();
		void reset();
		void touch();
		void tick();
		void ImGuiConfig();
		void ImGuiStatus();
		void ImGuiExpose();

	private:
		int pspHeight = 270;
		Resolution res;
	};

	extern _TrapPSP trap;
}
