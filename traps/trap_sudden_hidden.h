#pragma once
#include "trap.h"

namespace TrapSuhidden
{
	void installHooks();

	class _TrapSuhidden : public Trap
	{
	public:
		_TrapSuhidden() : Trap() {
			installHooks();
		};

		void config(const toml::table& settings);
		void save(toml::table& settings);
		void reset();
		void resetSudden();
		void resetHidden();
		void touch();
		void touchSudden();
		void touchSudden(bool force_overlap);
		void touchHidden();
		void touchHidden(bool force_overlap);
		bool getSudden() const;
		bool getHidden() const;
		void tick();
		void ImGuiConfig();
		void ImGuiStatus();
		void ImGuiExpose();

	private:
		bool overlap = false;

		bool isSudden = false;
		float timestampSudden = 0.0f;

		bool isHidden = false;
		float timestampHidden = 0.0f;
	};

	extern _TrapSuhidden trap;
}
