#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <libusb.h>

#include <u80211/kernel_interface.h>
#include <u80211_drv/status.h>
#include <u80211_drv/kernel_interface.h>

typedef struct {
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t condition;
	bool stopping;
	bool pending;
	bool destroy_on_exit;
	struct timespec deadline;
	u80211_kernel_work_fn_t function;
	void *context;
} u80211_test_work_t;

static int u80211_drv_kernel_status_from_libusb(int status) {
	if (status == LIBUSB_SUCCESS)
		return U80211_DRV_STATUS_SUCCESS;

	if (status == LIBUSB_ERROR_TIMEOUT)
		return U80211_DRV_STATUS_TIMEOUT;

	return U80211_DRV_STATUS_UNKNOWN_ERROR;
}

void *u80211_drv_kernel_allocate(size_t size) {
	return malloc(size);
}

void u80211_drv_kernel_free(void *memory) {
	free(memory);
}

int u80211_drv_kernel_get_firmware(const char *name, u80211_drv_kernel_firmware_callback_t callback, void *context) {
	int firmware_directory = open("firmware_blobs", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (firmware_directory < 0)
		return U80211_DRV_STATUS_UNKNOWN_ERROR;

	int firmware = openat(firmware_directory, name, O_RDONLY | O_CLOEXEC);
	close(firmware_directory);
	if (firmware < 0)
		return U80211_DRV_STATUS_UNKNOWN_ERROR;

	struct stat firmware_stat;
	if (fstat(firmware, &firmware_stat) != 0) {
		close(firmware);
		return U80211_DRV_STATUS_UNKNOWN_ERROR;
	}

	size_t firmware_size = (size_t)firmware_stat.st_size;
	void *firmware_data = malloc(firmware_size);
	if (firmware_data == NULL) {
		close(firmware);
		return U80211_DRV_STATUS_OUT_OF_MEMORY;
	}

	ssize_t read_size = read(firmware, firmware_data, firmware_size);
	if ((size_t)read_size != firmware_size) {
		free(firmware_data);
		close(firmware);
		return U80211_DRV_STATUS_UNKNOWN_ERROR;
	}

	close(firmware);
	callback(context, firmware_data, firmware_size);
	free(firmware_data);
	return U80211_DRV_STATUS_SUCCESS;
}

int u80211_drv_kernel_get_device_descriptor(u80211_drv_device_handle_t device, u80211_drv_device_descriptor_t *descriptor) {
	struct libusb_device_descriptor usb_descriptor;
	int status = libusb_get_device_descriptor(libusb_get_device(device), &usb_descriptor);
	if (status != LIBUSB_SUCCESS)
		return u80211_drv_kernel_status_from_libusb(status);

	descriptor->vendor_id = usb_descriptor.idVendor;
	descriptor->product_id = usb_descriptor.idProduct;
	return U80211_DRV_STATUS_SUCCESS;
}

int u80211_drv_kernel_get_interface_descriptor(u80211_drv_interface_handle_t interface, u80211_drv_interface_descriptor_t *descriptor) {
	const struct libusb_interface_descriptor *usb_descriptor = interface;

	descriptor->number = usb_descriptor->bInterfaceNumber;
	descriptor->class_code = usb_descriptor->bInterfaceClass;
	descriptor->subclass = usb_descriptor->bInterfaceSubClass;
	descriptor->protocol = usb_descriptor->bInterfaceProtocol;
	descriptor->endpoint_count = usb_descriptor->bNumEndpoints;
	return U80211_DRV_STATUS_SUCCESS;
}

int u80211_drv_kernel_get_endpoints(u80211_drv_interface_handle_t interface, u80211_drv_endpoint_descriptor_t *endpoints, size_t endpoint_count) {
	const struct libusb_interface_descriptor *usb_descriptor = interface;
	if (usb_descriptor == NULL || endpoint_count != usb_descriptor->bNumEndpoints || (endpoint_count != 0 && endpoints == NULL))
		return U80211_DRV_STATUS_UNKNOWN_ERROR;

	for (size_t i = 0; i < endpoint_count; ++i) {
		endpoints[i].address = usb_descriptor->endpoint[i].bEndpointAddress;
		endpoints[i].attributes = usb_descriptor->endpoint[i].bmAttributes;
		endpoints[i].maximum_packet_size = usb_descriptor->endpoint[i].wMaxPacketSize;
		endpoints[i].interval = usb_descriptor->endpoint[i].bInterval;
	}

	return U80211_DRV_STATUS_SUCCESS;
}

int u80211_drv_kernel_submit_control_xfer_and_wait(u80211_drv_device_handle_t device, uint8_t flags, uint8_t request, uint16_t value, uint16_t index, void *buf, uint16_t buffer_size, size_t *transferred_size, unsigned int timeout) {
	int result = libusb_control_transfer(device, flags, request, value, index, buf, buffer_size, timeout);
	if (result < 0)
		return u80211_drv_kernel_status_from_libusb(result);

	*transferred_size = result;
	return U80211_DRV_STATUS_SUCCESS;
}

int u80211_drv_kernel_submit_bulk_xfer_and_wait(u80211_drv_device_handle_t device, uint8_t endpoint_address, void *buf, size_t buffer_size, size_t *transferred_size, unsigned int timeout) {
	int transferred = 0;
	int status = libusb_bulk_transfer(device, endpoint_address, buf, buffer_size, &transferred, timeout);
	if (status != LIBUSB_SUCCESS)
		return u80211_drv_kernel_status_from_libusb(status);

	*transferred_size = transferred;
	return U80211_DRV_STATUS_SUCCESS;
}

void u80211_drv_kernel_stall_us(unsigned int microseconds) {
	struct timespec remaining = {
		.tv_sec = microseconds / 1000000,
		.tv_nsec = (long)(microseconds % 1000000) * 1000,
	};

	while (nanosleep(&remaining, &remaining) < 0 && errno == EINTR)
		;
}

#define U80211_DRV_KERNEL_PRINT_LEVEL_INFO 0
#define U80211_DRV_KERNEL_PRINT_LEVEL_WARN 1
#define U80211_DRV_KERNEL_PRINT_LEVEL_ERROR 2
void u80211_drv_kernel_print(int level, const char *msg) {
	switch (level) {
		case U80211_DRV_KERNEL_PRINT_LEVEL_INFO:
			fprintf(stderr, "[INFO] %s\n", msg);
			break;
		case U80211_DRV_KERNEL_PRINT_LEVEL_WARN:
			fprintf(stderr, "[WARN] %s\n", msg);
			break;
		case U80211_DRV_KERNEL_PRINT_LEVEL_ERROR:
			fprintf(stderr, "[ERROR] %s\n", msg);
			break;
		default:
			fprintf(stderr, "[UNKNOWN] %s\n", msg);
			break;
	}
}

static void u80211_test_destroy_work(u80211_test_work_t *work) {
	pthread_cond_destroy(&work->condition);
	pthread_mutex_destroy(&work->mutex);
	free(work);
}

static int u80211_test_timespec_compare(const struct timespec *left, const struct timespec *right) {
	if (left->tv_sec != right->tv_sec)
		return left->tv_sec < right->tv_sec ? -1 : 1;
	if (left->tv_nsec != right->tv_nsec)
		return left->tv_nsec < right->tv_nsec ? -1 : 1;
	return 0;
}

static struct timespec u80211_test_deadline_after_ms(size_t milliseconds) {
	struct timespec deadline;
	clock_gettime(CLOCK_MONOTONIC, &deadline);
	deadline.tv_sec += (time_t)(milliseconds / 1000);
	deadline.tv_nsec += (long)(milliseconds % 1000) * 1000000L;
	if (deadline.tv_nsec >= 1000000000L) {
		++deadline.tv_sec;
		deadline.tv_nsec -= 1000000000L;
	}
	return deadline;
}

static void *u80211_test_work_thread(void *argument) {
	u80211_test_work_t *work = argument;
	pthread_mutex_lock(&work->mutex);

	for (;;) {
		while (!work->stopping && !work->pending)
			pthread_cond_wait(&work->condition, &work->mutex);
		if (work->stopping)
			break;

		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		while (!work->stopping && u80211_test_timespec_compare(&now, &work->deadline) < 0) {
			int status = pthread_cond_timedwait(&work->condition, &work->mutex, &work->deadline);
			if (status != 0 && status != ETIMEDOUT)
				continue;
			clock_gettime(CLOCK_MONOTONIC, &now);
		}
		if (work->stopping)
			break;

		u80211_kernel_work_fn_t function = work->function;
		void *context = work->context;
		work->pending = false;
		pthread_mutex_unlock(&work->mutex);
		function(context);
		pthread_mutex_lock(&work->mutex);
	}

	bool destroy_on_exit = work->destroy_on_exit;
	pthread_mutex_unlock(&work->mutex);
	if (destroy_on_exit)
		u80211_test_destroy_work(work);
	return NULL;
}

void *u80211_kernel_allocate(size_t size) {
	return malloc(size);
}

void u80211_kernel_free(void *memory) {
	free(memory);
}

void *u80211_kernel_allocate_mutex(void) {
	pthread_mutex_t *mutex = malloc(sizeof(*mutex));
	if (mutex == NULL)
		return NULL;
	if (pthread_mutex_init(mutex, NULL) != 0) {
		free(mutex);
		return NULL;
	}
	return mutex;
}

void u80211_kernel_free_mutex(void *opaque_mutex) {
	pthread_mutex_t *mutex = opaque_mutex;
	pthread_mutex_destroy(mutex);
	free(mutex);
}

void u80211_kernel_acquire_mutex(void *mutex) {
	pthread_mutex_lock(mutex);
}

void u80211_kernel_release_mutex(void *mutex) {
	pthread_mutex_unlock(mutex);
}

void *u80211_kernel_allocate_semaphore(unsigned int initial_count) {
	sem_t *semaphore = malloc(sizeof(*semaphore));
	if (semaphore == NULL)
		return NULL;
	if (sem_init(semaphore, 0, initial_count) != 0) {
		free(semaphore);
		return NULL;
	}
	return semaphore;
}

void u80211_kernel_free_semaphore(void *opaque_semaphore) {
	sem_t *semaphore = opaque_semaphore;
	sem_destroy(semaphore);
	free(semaphore);
}

void u80211_kernel_wait_semaphore(void *semaphore) {
	while (sem_wait(semaphore) != 0 && errno == EINTR)
		;
}

void u80211_kernel_signal_semaphore(void *semaphore) {
	sem_post(semaphore);
}

void *u80211_kernel_allocate_spinlock(void) {
	void *memory = malloc(sizeof(pthread_spinlock_t));
	if (memory == NULL)
		return NULL;
	pthread_spinlock_t *spinlock = memory;
	if (pthread_spin_init(spinlock, PTHREAD_PROCESS_PRIVATE) != 0) {
		free(memory);
		return NULL;
	}
	return memory;
}

void u80211_kernel_free_spinlock(void *opaque_spinlock) {
	pthread_spinlock_t *spinlock = opaque_spinlock;
	pthread_spin_destroy(spinlock);
	free(opaque_spinlock);
}

void u80211_kernel_acquire_spinlock(void *spinlock) {
	pthread_spin_lock(spinlock);
}

void u80211_kernel_release_spinlock(void *spinlock) {
	pthread_spin_unlock(spinlock);
}

void *u80211_kernel_allocate_rwlock(void) {
	pthread_rwlock_t *rwlock = malloc(sizeof(*rwlock));
	if (rwlock == NULL)
		return NULL;
	if (pthread_rwlock_init(rwlock, NULL) != 0) {
		free(rwlock);
		return NULL;
	}
	return rwlock;
}

void u80211_kernel_free_rwlock(void *opaque_rwlock) {
	pthread_rwlock_t *rwlock = opaque_rwlock;
	pthread_rwlock_destroy(rwlock);
	free(rwlock);
}

void u80211_kernel_acquire_rwlock_exclusive(void *rwlock) {
	pthread_rwlock_wrlock(rwlock);
}

void u80211_kernel_acquire_rwlock_shared(void *rwlock) {
	pthread_rwlock_rdlock(rwlock);
}

void u80211_kernel_release_rwlock_exclusive(void *rwlock) {
	pthread_rwlock_unlock(rwlock);
}

void u80211_kernel_release_rwlock_shared(void *rwlock) {
	pthread_rwlock_unlock(rwlock);
}

void *u80211_kernel_allocate_work(void) {
	u80211_test_work_t *work = calloc(1, sizeof(*work));
	if (work == NULL)
		return NULL;
	if (pthread_mutex_init(&work->mutex, NULL) != 0) {
		free(work);
		return NULL;
	}

	pthread_condattr_t attributes;
	if (pthread_condattr_init(&attributes) != 0) {
		pthread_mutex_destroy(&work->mutex);
		free(work);
		return NULL;
	}
	if (pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC) != 0 ||
		pthread_cond_init(&work->condition, &attributes) != 0) {
		pthread_condattr_destroy(&attributes);
		pthread_mutex_destroy(&work->mutex);
		free(work);
		return NULL;
	}
	pthread_condattr_destroy(&attributes);

	if (pthread_create(&work->thread, NULL, u80211_test_work_thread, work) != 0) {
		pthread_cond_destroy(&work->condition);
		pthread_mutex_destroy(&work->mutex);
		free(work);
		return NULL;
	}
	return work;
}

void u80211_kernel_enqueue_work(void *opaque_work, u80211_kernel_work_fn_t function, void *context, size_t milliseconds) {
	u80211_test_work_t *work = opaque_work;
	pthread_mutex_lock(&work->mutex);
	if (!work->pending) {
		work->function = function;
		work->context = context;
		work->deadline = u80211_test_deadline_after_ms(milliseconds);
		work->pending = true;
		pthread_cond_signal(&work->condition);
	}
	pthread_mutex_unlock(&work->mutex);
}

void u80211_kernel_free_work(void *opaque_work) {
	u80211_test_work_t *work = opaque_work;
	bool destroy_on_exit = pthread_equal(pthread_self(), work->thread);

	pthread_mutex_lock(&work->mutex);
	work->stopping = true;
	work->destroy_on_exit = destroy_on_exit;
	pthread_cond_signal(&work->condition);
	pthread_mutex_unlock(&work->mutex);

	if (destroy_on_exit) {
		pthread_detach(work->thread);
		return;
	}
	pthread_join(work->thread, NULL);
	u80211_test_destroy_work(work);
}

void u80211_kernel_receive_callback(u80211_device_t *device, void *buffer, size_t size) {
	(void)device;
	(void)buffer;
	(void)size;
}
