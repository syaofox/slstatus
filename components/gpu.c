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
	typedef nvmlReturn_t (*nvmlDeviceGetTemperature_t)(nvmlDevice_t, unsigned int, unsigned int*);
	typedef const char* (*nvmlErrorString_t)(nvmlReturn_t);

	static void *nvml_handle = NULL;
	static nvmlInit_t nvmlInit_fn = NULL;
	static nvmlShutdown_t nvmlShutdown_fn = NULL;
	static nvmlDeviceGetHandleByIndex_t nvmlDeviceGetHandleByIndex_fn = NULL;
	static nvmlDeviceGetUtilizationRates_t nvmlDeviceGetUtilizationRates_fn = NULL;
	static nvmlDeviceGetMemoryInfo_t nvmlDeviceGetMemoryInfo_fn = NULL;
	static nvmlDeviceGetTemperature_t nvmlDeviceGetTemperature_fn = NULL;
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

		*(void **)(&nvmlInit_fn) = dlsym(nvml_handle, "nvmlInit");
		*(void **)(&nvmlShutdown_fn) = dlsym(nvml_handle, "nvmlShutdown");
		*(void **)(&nvmlDeviceGetHandleByIndex_fn) = dlsym(nvml_handle, "nvmlDeviceGetHandleByIndex");
		*(void **)(&nvmlDeviceGetUtilizationRates_fn) = dlsym(nvml_handle, "nvmlDeviceGetUtilizationRates");
		*(void **)(&nvmlDeviceGetMemoryInfo_fn) = dlsym(nvml_handle, "nvmlDeviceGetMemoryInfo");
		*(void **)(&nvmlDeviceGetTemperature_fn) = dlsym(nvml_handle, "nvmlDeviceGetTemperature");

		if (!nvmlInit_fn || !nvmlShutdown_fn || !nvmlDeviceGetHandleByIndex_fn ||
		    !nvmlDeviceGetUtilizationRates_fn || !nvmlDeviceGetMemoryInfo_fn ||
		    !nvmlDeviceGetTemperature_fn) {
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
	gpu_perc(const char *unused)
	{
		nvmlUtilization_t utilization;

		if (!init_nvml())
			return NULL;

		if (nvmlDeviceGetUtilizationRates_fn(device, &utilization) != NVML_SUCCESS)
			return NULL;

		return bprintf("%u%%", utilization.gpu);
	}

	const char *
	gpu_temp(const char *unused)
	{
		unsigned int temp;

		if (!init_nvml())
			return NULL;

		if (nvmlDeviceGetTemperature_fn(device, 0, &temp) != NVML_SUCCESS)
			return NULL;

		return bprintf("%u°C", temp);
	}

	const char *
	gpu_vram(const char *unused)
	{
		nvmlMemory_t memory;
		double vram_gb;

		if (!init_nvml())
			return NULL;

		if (nvmlDeviceGetMemoryInfo_fn(device, &memory) != NVML_SUCCESS)
			return NULL;

		vram_gb = (double)memory.used / 1024.0 / 1024.0 / 1024.0;

		return bprintf("%.2fG", vram_gb);
	}

	const char *
	gpu_combined(const char *unused)
	{
		nvmlUtilization_t utilization;
		nvmlMemory_t memory;
		unsigned int temp;

		if (!init_nvml())
			return NULL;

		if (nvmlDeviceGetUtilizationRates_fn(device, &utilization) != NVML_SUCCESS)
			return NULL;

		if (nvmlDeviceGetMemoryInfo_fn(device, &memory) != NVML_SUCCESS)
			return NULL;

		if (nvmlDeviceGetTemperature_fn(device, 0, &temp) != NVML_SUCCESS)
			return NULL;

		return bprintf("GPU %u%% %u°C RAM %.2fG", utilization.gpu, temp, (double)memory.used / 1024.0 / 1024.0 / 1024.0);
	}
#else
	const char *
	gpu_perc(const char *unused)
	{
		return NULL;
	}

	const char *
	gpu_temp(const char *unused)
	{
		return NULL;
	}

	const char *
	gpu_vram(const char *unused)
	{
		return NULL;
	}

	const char *
	gpu_combined(const char *unused)
	{
		return NULL;
	}
#endif
