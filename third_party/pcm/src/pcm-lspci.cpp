// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2017-2022, Intel Corporation

// written by Patrick Lu
#include "cpucounters.h"
#ifdef _MSC_VER
#pragma warning(disable : 4996) // for sprintf
#include <windows.h>
#include "windows/windriver.h"
#else
#include <unistd.h>
#endif
#include <iostream>
#include <stdlib.h>
#include <cstdint>
#ifdef _MSC_VER
#include "freegetopt/getopt.h"
#endif

#include "lspci.h"
using namespace std;
using namespace pcm;

void scanBus(int bus, const PCIDB & pciDB)
{
    if(!PciHandleType::exists(0, bus, 8, 2)) return;

    std::cout << "BUS 0x" << std::hex << bus << std::dec << "\n";

    struct iio_skx iio_skx;

    PciHandleType h(0, bus, 8, 2);
    uint32 cpubusno = 0;

    h.read32(0xcc, &cpubusno); // CPUBUSNO register
    iio_skx.stacks[0].busno = cpubusno & 0xff;
    iio_skx.stacks[1].busno = (cpubusno >> 8) & 0xff;
    iio_skx.stacks[2].busno = (cpubusno >> 16) & 0xff;
    iio_skx.stacks[3].busno = (cpubusno >> 24) & 0xff;
    h.read32(0xd0, &cpubusno); // CPUBUSNO1 register
    iio_skx.stacks[4].busno = cpubusno & 0xff;
    iio_skx.stacks[5].busno = (cpubusno >> 8) & 0xff;

    for (uint8_t stack = 0; stack < 6; stack++) {
        uint8_t busno = iio_skx.stacks[stack].busno;
        std::cout << "stack" << unsigned(stack) << std::hex << ":0x" << unsigned(busno) << std::dec << ",(" << unsigned(busno) << ")\n";
        for (uint8_t part = 0; part < 3; part++) {
                struct pci *pci = &iio_skx.stacks[stack].parts[part].root_pci_dev;
                struct bdf *bdf = &pci->bdf;
                bdf->busno = busno;
                bdf->devno = part;
                bdf->funcno = 0;
                if (stack != 0 && busno == 0) /* This is a workaround to catch some IIO stack does not exist */
                    pci->exist = false;
                else
                    probe_pci(pci);
            }
        }
    for (uint8_t stack = 0; stack < 6; stack++) {
        for (uint8_t part = 0; part < 4; part++) {
            struct pci p = iio_skx.stacks[stack].parts[part].root_pci_dev;
            if (!p.exist)
                continue;
            for (uint8_t b = p.secondary_bus_number; b <= p.subordinate_bus_number; b++) { /* FIXME: for 0:0.0, we may need to scan from secondary switch down; lgtm [cpp/fixme-comment] */
                for (uint8_t d = 0; d < 32; d++) {
                    for (uint8_t f = 0; f < 8; f++) {
                        struct pci pci;
                        pci.exist = false;
                        pci.bdf.busno = b;
                        pci.bdf.devno = d;
                        pci.bdf.funcno = f;
                        probe_pci(&pci);
                        if (pci.exist)
                            iio_skx.stacks[stack].parts[part].child_pci_devs.push_back(pci);
                    }
                }
            }
        }
    }

    for (uint8_t stack = 1; stack < 6; stack++) { /* XXX: Maybe there is no point to display all built-in devices on DMI/CBDMA stacks, if so, change stack = 1 */
        for (uint8_t part = 0; part < 4; part++) {
            vector<struct pci> v = iio_skx.stacks[stack].parts[part].child_pci_devs;
            struct pci pp = iio_skx.stacks[stack].parts[part].root_pci_dev;
            if (pp.exist)
                print_pci(pp, pciDB);
            for (vector<struct pci>::const_iterator iunit = v.begin(); iunit != v.end(); ++iunit) {
                struct pci p = *iunit;
                if (p.exist)
                    print_pci(p, pciDB);
            }
        }
    }
}


PCM_MAIN_NOTHROW;

int mainThrows(int /*argc*/, char * /*argv*/[])
{
    PCIDB pciDB;
    load_PCIDB(pciDB);
    PCM * m = PCM::getInstance();

    if (!m->isSkxCompatible())
    {
        cerr << "Unsupported processor model (" << m->getCPUModel() << ").\n";
        exit(EXIT_FAILURE);
    }
    std::cout << "\n Display PCI tree information\n\n";
    for(int bus=0; bus < 256; ++bus)
        scanBus(bus, pciDB);
    return 0;
}
