#pragma once

#include <stddef.h>
#include <stdint.h>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL (-1)

enum esp_partition_type_t { ESP_PARTITION_TYPE_APP = 0, ESP_PARTITION_TYPE_DATA = 1 };
typedef int esp_partition_subtype_t;
typedef uint32_t spi_flash_mmap_handle_t;
enum spi_flash_mmap_memory_t { SPI_FLASH_MMAP_DATA = 0, SPI_FLASH_MMAP_INST };

struct esp_partition_t {
  esp_partition_type_t type;
  esp_partition_subtype_t subtype;
  uint32_t address;
  uint32_t size;
  char label[17];
};

const esp_partition_t* esp_partition_find_first(esp_partition_type_t type, esp_partition_subtype_t subtype,
                                                const char* label);
esp_err_t esp_partition_read(const esp_partition_t* p, size_t off, void* dst, size_t n);
esp_err_t esp_partition_write(const esp_partition_t* p, size_t off, const void* src, size_t n);
esp_err_t esp_partition_erase_range(const esp_partition_t* p, size_t off, size_t n);
esp_err_t esp_partition_mmap(const esp_partition_t* p, size_t off, size_t n, spi_flash_mmap_memory_t mem,
                             const void** out, spi_flash_mmap_handle_t* handle);
