#pragma once


#include <stdint.h>


struct rsdp_desc {
    char signature[8];
    uint8_t checksum;
    char oemid[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed));

struct rsdp_desc_20 {
    struct rsdp_desc base;

    uint32_t length;
    uint64_t xsdt_address;
    uint8_t ext_checksum;
    uint8_t _reserved[3];
} __attribute__((packed));


struct acpi_sdt_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oemid[6];
    char oem_tableid[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));


struct rsdt {
    struct acpi_sdt_header header;
    uint32_t pointers[];
};


struct madt {
    struct acpi_sdt_header header;
    uint32_t lapic_addr;
    uint32_t flags;
    uint8_t entries;
};

static inline int acpi_checksum(struct acpi_sdt_header * header) {
    uint8_t check = 0;
    for(size_t i = 0; i < header->length; i++) {
        check += ((uint8_t *)header)[i];
    }
    return check == 0;
}


static inline void outw(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}
