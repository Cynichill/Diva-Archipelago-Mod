#pragma once

struct DivaString {
	union {
		char str[16];
		char* ptr;
	};

	size_t length;
	size_t capacity;

	const bool is_ptr() const {
		return capacity >= 16;
	}

	const char* data() const {
		return is_ptr() ? ptr : str;
	}

	const std::string to_string() const {
		return std::string(is_ptr() ? ptr : str, length);
	}
};

struct SFXdata {
	int id;
	int unk;
	DivaString name;
	DivaString file;
};

struct SFXchainslide {
	int id;
	int unk;
	DivaString name;
	DivaString chainslide_first_name;
	DivaString chainslide_sub_name;
	DivaString chainslide_success_name;
	DivaString chainslide_failure_name;
};

struct SFXChainslideList {
	SFXchainslide* data;
	int count;
};

struct SFXList {
	SFXdata* data;
	int count;
};

struct PvPlayData_SFX { // PvPlayData + 0x2CF18
	DivaString se_name; // button
	DivaString pvbranch_success_se_name;
	DivaString slide_name; // flick
	DivaString chainslide_first_name;
	DivaString chainslide_sub_name;
	DivaString chainslide_success_name;
	DivaString chainslide_failure_name;
};

auto StringFree = reinterpret_cast<DivaString * (__fastcall*)(DivaString*)>(0x14014BCD0);
auto StringInit = reinterpret_cast<DivaString * (__fastcall*)(DivaString*, const char*, size_t)>(0x14014ba50);

typedef enum _DIVA_DIFFICULTY : uint32_t {
    Easy = 0x0,
    Normal = 0x1,
    Hard = 0x2,
    Extreme = 0x3,
    ExExtreme = 0x4,
} DIVA_DIFFICULTY;
typedef enum _DIVA_GRADE : uint32_t {
    Failed = 0x0,
    Cheap = 0x1,
    Standard = 0x2,
    Great = 0x3,
    Excellent = 0x4,
    Perfect = 0x4
} DIVA_GRADE;
typedef struct _DIVA_PV_DIF {
    unsigned int Difficulty;
} DIVA_PV_DIF;
typedef struct _DIVA_PV_ID {
    unsigned int Id;
} DIVA_PV_ID;
typedef struct _DIVA_STAT {
    float CompletionRate;
} DIVA_STAT;
typedef struct _DIVA_SCORE {
    unsigned int TotalScore;
    unsigned int Unknown1;
    unsigned int Unknown2;
    unsigned int Unknown3;
    unsigned int Unknown4;
    unsigned int Unknown5;
    unsigned int Unknown6;
    unsigned int Unknown7;
    unsigned int Unknown8;
    unsigned int Combo;
    unsigned int preAdjustCool;
    unsigned int preAdjustFine;
    unsigned int preAdjustSafe;
    unsigned int preAdjustSad;
    unsigned int preAdjustWorst;
    unsigned int Cool;
    unsigned int Fine;
    unsigned int Safe;
    unsigned int Sad;
    unsigned int Worst;
} DIVA_SCORE;
