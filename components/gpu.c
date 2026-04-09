/* See LICENSE file for copyright and license details. */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../slstatus.h"
#include "../util.h"

#if defined(__linux__)
	typedef enum {
		NVML_SUCCESS = 0,
		NVML_ERROR_UNINITIALIZED = 1,
		NVML_ERROR_INVALID_ARGUMENT = 2,
		NVML_ERROR_NOT_SUPPORTED = 3,
		NVML_ERROR_NO_PERMISSION = 4,
		NVML_ERROR_ALREADY_INITIALIZED = 5,
		NVML_ERROR_NOT_FOUND = 6,
		NVML_ERROR_INSUFFICIENT_SIZE = 7,
		NVML_ERROR_INSUFFICIENT_POWER = 8,
		NVML_ERROR_DRIVER_NOT_LOADED = 9,
		NVML_ERROR_TIMEOUT = 10,
		NVML_ERROR_IRQ_ISSUE = 11,
		NVML_ERROR_LIBRARY_NOT_FOUND = 12,
		NVML_ERROR_FUNCTION_NOT_FOUND = 13,
		NVML_ERROR_CORRUPTED_INFOROM = 14,
		NVML_ERROR_GPU_IS_LOST = 15,
		NVML_ERROR_UNKNOWN = 999
	} nvmlReturn_t;

	typedef struct nvmlDevice_st* nvmlDevice_t;
	typedef struct {
		unsigned int gpu;
		unsigned int memory;
	} nvmlUtilization_t;

	typedef struct {
		unsigned long long total;
		unsigned long long reserved;
		unsigned long long used;
		unsigned long long free;
	} nvmlMemory_t;

	typedef nvmlReturn_t (*nvmlInit_t)(void);
	typedef nvmlReturn_t (*nvmlShutdown_t)(void);
	typedef nvmlReturn_t (*nvmlDeviceGetHandleByIndex_t)(unsigned int, nvmlDevice_t*);
	typedef nvmlReturn_t (*nvmlDeviceGetUtilizationRates_t)(nvmlDevice_t, nvmlUtilization_t*);
	typedef nvmlReturn_t (*nvmlDeviceGetMemoryInfo_t)(nvmlDevice_t, nvmlMemory_t*);
	typedef const char* (*nvmlErrorString_t)(nvmlReturn_t);

	static void *nvml_handle = NULL;
	static nvmlInit_t nvmlInit_fn = NULL;
	static nvmlShutdown_t nvmlShutdown_fn = NULL;
	static nvmlDeviceGetHandleByIndex_t nvmlDeviceGetHandleByIndex_fn = NULL;
	static nvmlDeviceGetUtilizationRates_t nvmlDeviceGetUtilizationRates_fn = NULL;
	static nvmlDeviceGetMemoryInfo_t nvmlDeviceGetMemoryInfo_fn = NULL;
	static nvmlDevice_t device = NULL;
	static int nvml_initialized = 0;

	static int
	load_nvml(void)
	{
		if (nvml_handle)
			return 1;

		nvml_handle = dlopen("libnvidia-ml.so.1", RTLD_LAZY);
		if (!nvml_handle)
			return 0;

		nvmlInit_fn = (nvmlInit_t)dlsym(nvml_handle, "nvmlInit");
		nvmlShutdown_fn = (nvmlShutdown_t)dlsym(nvml_handle, "nvmlShutdown");
		nvmlDeviceGetHandleByIndex_fn = (nvmlDeviceGetHandleByIndex_t)dlsym(nvml_handle, "nvmlDeviceGetHandleByIndex");
		nvmlDeviceGetUtilizationRates_fn = (nvmlDeviceGetUtilizationRates_t)dlsym(nvml_handle, "nvmlDeviceGetUtilizationRates");
		nvmlDeviceGetMemoryInfo_fn = (nvmlDeviceGetMemoryInfo_t)dlsym(nvml_handle, "nvmlDeviceGetMemoryInfo");

		if (!nvmlInit_fn || !nvmlShutdown_fn || !nvmlDeviceGetHandleByIndex_fn ||
		    !nvmlDeviceGetUtilizationRates_fn || !nvmlDeviceGetMemoryInfo_fn) {
			dlclose(nvml_handle);
			nvml_handle = NULL;
			return 0;
		}

		return 1;
	}

	static int
	init_nvml(void)
	{
		if (nvml_initialized)
			return 1;

		if (!load_nvml())
			return 0;

		if (nvmlInit_fn() != NVML_SUCCESS)
			return 0;

		if (nvmlDeviceGetHandleByIndex_fn(0, &device) != NVML_SUCCESS) {
			nvmlShutdown_fn();
			return 0;
		}

		nvml_initialized = 1;
		return 1;
	}

	const char *
	gpu_combined(const char *unused)
	{
		nvmlUtilization_t utilization;
		nvmlMemory_t memory;
		unsigned int gpu_util;
		double vram_gb;

		if (!init_nvml())
			return NULL;

		if (nvmlDeviceGetUtilizationRates_fn(device, &utilization) != NVML_SUCCESS)
			return NULL;

		if (nvmlDeviceGetMemoryInfo_fn(device, &memory) != NVML_SUCCESS)
			return NULL;

		gpu_util = utilization.gpu;
		vram_gb = (double)memory.used / 1024.0 / 1024.0 / 1024.0;

		return bprintf("GPU %u%% VRAM %.2fG", gpu_util, vram_gb);
	}
#else
	const char *
	gpu_combined(const char *unused)
	{
		return NULL;
	}
#endif