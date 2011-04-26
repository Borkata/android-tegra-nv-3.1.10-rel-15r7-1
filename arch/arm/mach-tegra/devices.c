/*
 * arch/arm/mach-tegra/devices.c
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@android.com>
 *	Erik Gilling <ccross@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#include <linux/resource.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/fsl_devices.h>
#include <linux/serial_8250.h>
#include <asm/pmu.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dma.h>
#include <mach/sata.h>

static struct resource i2c_resource1[] = {
	[0] = {
		.start  = INT_I2C,
		.end    = INT_I2C,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_I2C_BASE,
		.end	= TEGRA_I2C_BASE + TEGRA_I2C_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource i2c_resource2[] = {
	[0] = {
		.start  = INT_I2C2,
		.end    = INT_I2C2,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_I2C2_BASE,
		.end	= TEGRA_I2C2_BASE + TEGRA_I2C2_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource i2c_resource3[] = {
	[0] = {
		.start  = INT_I2C3,
		.end    = INT_I2C3,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_I2C3_BASE,
		.end	= TEGRA_I2C3_BASE + TEGRA_I2C3_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
static struct resource i2c_resource4[] = {
	[0] = {
		.start  = INT_DVC,
		.end    = INT_DVC,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_DVC_BASE,
		.end	= TEGRA_DVC_BASE + TEGRA_DVC_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

#elif defined(CONFIG_ARCH_TEGRA_3x_SOC)
static struct resource i2c_resource4[] = {
	[0] = {
		.start  = INT_I2C4,
		.end    = INT_I2C4,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_I2C4_BASE,
		.end	= TEGRA_I2C4_BASE + TEGRA_I2C4_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource i2c_resource5[] = {
	[0] = {
		.start  = INT_I2C5,
		.end    = INT_I2C5,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_I2C5_BASE,
		.end	= TEGRA_I2C5_BASE + TEGRA_I2C5_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};
#endif

struct platform_device tegra_i2c_device1 = {
	.name		= "tegra-i2c",
	.id		= 0,
	.resource	= i2c_resource1,
	.num_resources	= ARRAY_SIZE(i2c_resource1),
	.dev = {
		.platform_data = 0,
	},
};

struct platform_device tegra_i2c_device2 = {
	.name		= "tegra-i2c",
	.id		= 1,
	.resource	= i2c_resource2,
	.num_resources	= ARRAY_SIZE(i2c_resource2),
	.dev = {
		.platform_data = 0,
	},
};

struct platform_device tegra_i2c_device3 = {
	.name		= "tegra-i2c",
	.id		= 2,
	.resource	= i2c_resource3,
	.num_resources	= ARRAY_SIZE(i2c_resource3),
	.dev = {
		.platform_data = 0,
	},
};

struct platform_device tegra_i2c_device4 = {
	.name		= "tegra-i2c",
	.id		= 3,
	.resource	= i2c_resource4,
	.num_resources	= ARRAY_SIZE(i2c_resource4),
	.dev = {
		.platform_data = 0,
	},
};

#ifdef CONFIG_ARCH_TEGRA_3x_SOC
struct platform_device tegra_i2c_device5 = {
	.name		= "tegra-i2c",
	.id		= 4,
	.resource	= i2c_resource5,
	.num_resources	= ARRAY_SIZE(i2c_resource5),
	.dev = {
		.platform_data = 0,
	},
};
#endif

static struct resource spi_resource1[] = {
	[0] = {
		.start  = INT_SPI_1,
		.end    = INT_SPI_1,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SPI1_BASE,
		.end	= TEGRA_SPI1_BASE + TEGRA_SPI1_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource spi_resource2[] = {
	[0] = {
		.start  = INT_SPI_2,
		.end    = INT_SPI_2,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SPI2_BASE,
		.end	= TEGRA_SPI2_BASE + TEGRA_SPI2_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource spi_resource3[] = {
	[0] = {
		.start  = INT_SPI_3,
		.end    = INT_SPI_3,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SPI3_BASE,
		.end	= TEGRA_SPI3_BASE + TEGRA_SPI3_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource spi_resource4[] = {
	[0] = {
		.start  = INT_SPI_4,
		.end    = INT_SPI_4,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SPI4_BASE,
		.end	= TEGRA_SPI4_BASE + TEGRA_SPI4_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};
#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
static struct resource spi_resource5[] = {
	[0] = {
		.start  = INT_SPI_5,
		.end    = INT_SPI_5,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SPI5_BASE,
		.end	= TEGRA_SPI5_BASE + TEGRA_SPI5_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource spi_resource6[] = {
	[0] = {
		.start  = INT_SPI_6,
		.end    = INT_SPI_6,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SPI6_BASE,
		.end	= TEGRA_SPI6_BASE + TEGRA_SPI6_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};
#endif

struct platform_device tegra_spi_device1 = {
	.name           = "spi_tegra",
	.id             = 0,
	.resource       = spi_resource1,
	.num_resources  = ARRAY_SIZE(spi_resource1),
	.dev  = {
		.coherent_dma_mask      = 0xffffffff,
	},
};

struct platform_device tegra_spi_device2 = {
	.name           = "spi_tegra",
	.id             = 1,
	.resource       = spi_resource2,
	.num_resources  = ARRAY_SIZE(spi_resource2),
	.dev  = {
		.coherent_dma_mask      = 0xffffffff,
	},
};

struct platform_device tegra_spi_device3 = {
	.name           = "spi_tegra",
	.id             = 2,
	.resource       = spi_resource3,
	.num_resources  = ARRAY_SIZE(spi_resource3),
	.dev  = {
		.coherent_dma_mask      = 0xffffffff,
	},
};

struct platform_device tegra_spi_device4 = {
	.name           = "spi_tegra",
	.id             = 3,
	.resource       = spi_resource4,
	.num_resources  = ARRAY_SIZE(spi_resource4),
	.dev  = {
		.coherent_dma_mask      = 0xffffffff,
	},
};
#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
struct platform_device tegra_spi_device5 = {
	.name           = "spi_tegra",
	.id             = 4,
	.resource       = spi_resource5,
	.num_resources  = ARRAY_SIZE(spi_resource5),
	.dev  = {
		.coherent_dma_mask      = 0xffffffff,
	},
};

struct platform_device tegra_spi_device6 = {
	.name           = "spi_tegra",
	.id             = 5,
	.resource       = spi_resource6,
	.num_resources  = ARRAY_SIZE(spi_resource6),
	.dev  = {
		.coherent_dma_mask      = 0xffffffff,
	},
};
#endif


static struct resource sdhci_resource1[] = {
	[0] = {
		.start  = INT_SDMMC1,
		.end    = INT_SDMMC1,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SDMMC1_BASE,
		.end	= TEGRA_SDMMC1_BASE + TEGRA_SDMMC1_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource sdhci_resource2[] = {
	[0] = {
		.start  = INT_SDMMC2,
		.end    = INT_SDMMC2,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SDMMC2_BASE,
		.end	= TEGRA_SDMMC2_BASE + TEGRA_SDMMC2_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource sdhci_resource3[] = {
	[0] = {
		.start  = INT_SDMMC3,
		.end    = INT_SDMMC3,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SDMMC3_BASE,
		.end	= TEGRA_SDMMC3_BASE + TEGRA_SDMMC3_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource sdhci_resource4[] = {
	[0] = {
		.start  = INT_SDMMC4,
		.end    = INT_SDMMC4,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SDMMC4_BASE,
		.end	= TEGRA_SDMMC4_BASE + TEGRA_SDMMC4_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

/* board files should fill in platform_data register the devices themselvs.
 * See board-harmony.c for an example
 */
struct platform_device tegra_sdhci_device1 = {
	.name		= "sdhci-tegra",
	.id		= 0,
	.resource	= sdhci_resource1,
	.num_resources	= ARRAY_SIZE(sdhci_resource1),
};

struct platform_device tegra_sdhci_device2 = {
	.name		= "sdhci-tegra",
	.id		= 1,
	.resource	= sdhci_resource2,
	.num_resources	= ARRAY_SIZE(sdhci_resource2),
};

struct platform_device tegra_sdhci_device3 = {
	.name		= "sdhci-tegra",
	.id		= 2,
	.resource	= sdhci_resource3,
	.num_resources	= ARRAY_SIZE(sdhci_resource3),
};

struct platform_device tegra_sdhci_device4 = {
	.name		= "sdhci-tegra",
	.id		= 3,
	.resource	= sdhci_resource4,
	.num_resources	= ARRAY_SIZE(sdhci_resource4),
};

static struct resource w1_resources[] = {
	[0] = {
		.start = INT_OWR,
		.end   = INT_OWR,
		.flags = IORESOURCE_IRQ
	},
	[1] = {
		.start = TEGRA_OWR_BASE,
		.end = TEGRA_OWR_BASE + TEGRA_OWR_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_w1_device = {
	.name          = "tegra_w1",
	.id            = -1,
	.resource      = w1_resources,
	.num_resources = ARRAY_SIZE(w1_resources),
};

static struct resource tegra_udc_resources[] = {
	[0] = {
		.start	= TEGRA_USB_BASE,
		.end	= TEGRA_USB_BASE + TEGRA_USB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_USB,
		.end	= INT_USB,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource tegra_usb1_resources[] = {
	[0] = {
		.start	= TEGRA_USB_BASE,
		.end	= TEGRA_USB_BASE + TEGRA_USB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_USB,
		.end	= INT_USB,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource tegra_usb2_resources[] = {
	[0] = {
		.start	= TEGRA_USB2_BASE,
		.end	= TEGRA_USB2_BASE + TEGRA_USB2_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_USB2,
		.end	= INT_USB2,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource tegra_usb3_resources[] = {
	[0] = {
		.start	= TEGRA_USB3_BASE,
		.end	= TEGRA_USB3_BASE + TEGRA_USB3_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_USB3,
		.end	= INT_USB3,
		.flags	= IORESOURCE_IRQ,
	},
};

static u64 tegra_udc_dmamask = DMA_BIT_MASK(32);

static struct fsl_usb2_platform_data tegra_udc_pdata = {
	.operating_mode	= FSL_USB2_DR_DEVICE,
	.phy_mode	= FSL_USB2_PHY_UTMI,
};

struct platform_device tegra_udc_device = {
	.name	= "fsl-tegra-udc",
	.id	= -1,
	.dev	= {
		.dma_mask	= &tegra_udc_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data	= &tegra_udc_pdata,
	},
	.resource = tegra_udc_resources,
	.num_resources = ARRAY_SIZE(tegra_udc_resources),
};

static u64 tegra_ehci_dmamask = DMA_BIT_MASK(32);

struct platform_device tegra_ehci1_device = {
	.name	= "tegra-ehci",
	.id	= 0,
	.dev	= {
		.dma_mask	= &tegra_ehci_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
	.resource = tegra_usb1_resources,
	.num_resources = ARRAY_SIZE(tegra_usb1_resources),
};

struct platform_device tegra_ehci2_device = {
	.name	= "tegra-ehci",
	.id	= 1,
	.dev	= {
		.dma_mask	= &tegra_ehci_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
	.resource = tegra_usb2_resources,
	.num_resources = ARRAY_SIZE(tegra_usb2_resources),
};

struct platform_device tegra_ehci3_device = {
	.name	= "tegra-ehci",
	.id	= 2,
	.dev	= {
		.dma_mask	= &tegra_ehci_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
	.resource = tegra_usb3_resources,
	.num_resources = ARRAY_SIZE(tegra_usb3_resources),
};

static struct resource tegra_otg_resources[] = {
	[0] = {
		.start	= TEGRA_USB_BASE,
		.end	= TEGRA_USB_BASE + TEGRA_USB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_USB,
		.end	= INT_USB,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device tegra_otg_device = {
	.name		= "tegra-otg",
	.id		= -1,
	.resource	= tegra_otg_resources,
	.num_resources	= ARRAY_SIZE(tegra_otg_resources),
};

#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
static struct resource i2s_resource1[] = {
	[0] = {
		.start	= INT_I2S1,
		.end	= INT_I2S1,
		.flags	= IORESOURCE_IRQ
	},
	[1] = {
		.start	= TEGRA_DMA_REQ_SEL_I2S_1,
		.end	= TEGRA_DMA_REQ_SEL_I2S_1,
		.flags	= IORESOURCE_DMA
	},
	[2] = {
		.start	= TEGRA_I2S1_BASE,
		.end	= TEGRA_I2S1_BASE + TEGRA_I2S1_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device1 = {
	.name		= "i2s",
	.id		= 0,
	.resource	= i2s_resource1,
	.num_resources	= ARRAY_SIZE(i2s_resource1),
};

static struct resource i2s_resource2[] = {
	[0] = {
		.start	= INT_I2S2,
		.end	= INT_I2S2,
		.flags	= IORESOURCE_IRQ
	},
	[1] = {
		.start	= TEGRA_DMA_REQ_SEL_I2S2_1,
		.end	= TEGRA_DMA_REQ_SEL_I2S2_1,
		.flags	= IORESOURCE_DMA
	},
	[2] = {
		.start	= TEGRA_I2S2_BASE,
		.end	= TEGRA_I2S2_BASE + TEGRA_I2S2_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device2 = {
	.name		= "i2s",
	.id		= 1,
	.resource	= i2s_resource2,
	.num_resources	= ARRAY_SIZE(i2s_resource2),
};

static struct resource spdif_resource[] = {
	[0] = {
		.start	= INT_SPDIF,
		.end	= INT_SPDIF,
		.flags	= IORESOURCE_IRQ
	},
	[1] = {
		.start	= TEGRA_DMA_REQ_SEL_SPD_I,
		.end	= TEGRA_DMA_REQ_SEL_SPD_I,
		.flags	= IORESOURCE_DMA
	},
	[2] = {
		.start	= TEGRA_SPDIF_BASE,
		.end	= TEGRA_SPDIF_BASE + TEGRA_SPDIF_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

#elif defined(CONFIG_ARCH_TEGRA_3x_SOC)
static struct resource audio_resource[] = {
	[0] = {
		.start	= TEGRA_AUDIO_CLUSTER_BASE,
		.end	= TEGRA_AUDIO_CLUSTER_BASE + TEGRA_AUDIO_CLUSTER_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_audio_device = {
	.name		= "audio",
	.id		= -1,
	.resource	= audio_resource,
	.num_resources	= ARRAY_SIZE(audio_resource),
};

/* FIXME : Temporarly adding - find the right solution */

static struct resource spdif_resource[] = {
	[0] = {
		.start	= TEGRA_DMA_REQ_SEL_APBIF_CH3,
		.end	= TEGRA_DMA_REQ_SEL_APBIF_CH3,
		.flags	= IORESOURCE_DMA
	},
	[1] = {
		.start	= TEGRA_SPDIF_BASE,
		.end	= TEGRA_SPDIF_BASE + TEGRA_SPDIF_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};
#endif

struct platform_device tegra_spdif_device = {
	.name		= "spdif_out",
	.id		= -1,
	.resource	= spdif_resource,
	.num_resources	= ARRAY_SIZE(spdif_resource),
};

#if defined(CONFIG_SND_HDA_TEGRA)
static u64 tegra_hda_dma_mask = DMA_BIT_MASK(32);

static struct resource tegra_hda_resources[] = {
	[0] = {
		.start	= TEGRA_HDA_BASE,
		.end	= TEGRA_HDA_BASE + TEGRA_HDA_SIZE - 1 ,
		.flags	= IORESOURCE_MEM
	},
	[1] = {
		.start	= INT_HDA,
		.end	= INT_HDA,
		.flags	= IORESOURCE_IRQ
	},
};

struct platform_device tegra_hda_device = {
	.name		= "tegra-hda",
	.id		= 0,
	.dev = {
		.coherent_dma_mask	= DMA_BIT_MASK(32),
		.dma_mask		= &tegra_hda_dma_mask,
	},
	.resource	= tegra_hda_resources,
	.num_resources	= ARRAY_SIZE(tegra_hda_resources),
};
#endif

#ifdef CONFIG_SATA_AHCI_TEGRA
static u64 tegra_sata_dma_mask = DMA_BIT_MASK(32);

static struct tegra_sata_platform_data tegra_sata_pdata;

static struct resource tegra_sata_resources[] = {
	[0] = {
		.start = TEGRA_SATA_BAR5_BASE,
		.end = TEGRA_SATA_BAR5_BASE + TEGRA_SATA_BAR5_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = TEGRA_SATA_CONFIG_BASE,
		.end = TEGRA_SATA_CONFIG_BASE + TEGRA_SATA_CONFIG_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[2] = {
		.start = INT_SATA_CTL,
		.end = INT_SATA_CTL,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device tegra_sata_device = {
	.name 	= "tegra-sata",
	.id 	= 0,
	.dev 	= {
		.platform_data = &tegra_sata_pdata,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.dma_mask = &tegra_sata_dma_mask,
	},
	.resource = tegra_sata_resources,
	.num_resources = ARRAY_SIZE(tegra_sata_resources),
};
#endif

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
static struct resource das_resource[] = {
	[0] = {
		.start	= TEGRA_APB_MISC_BASE,
		.end	= TEGRA_APB_MISC_BASE + TEGRA_APB_MISC_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_das_device = {
	.name		= "tegra_das",
	.id		= -1,
	.resource	= das_resource,
	.num_resources	= ARRAY_SIZE(das_resource),
};
#endif

#if defined(CONFIG_TEGRA_IOVMM_GART)
static struct resource tegra_gart_resources[] = {
	[0] = {
		.name	= "mc",
		.flags	= IORESOURCE_MEM,
		.start	= TEGRA_MC_BASE,
		.end	= TEGRA_MC_BASE + TEGRA_MC_SIZE - 1,
	},
	[1] = {
		.name	= "gart",
		.flags	= IORESOURCE_MEM,
		.start	= TEGRA_GART_BASE,
		.end	= TEGRA_GART_BASE + TEGRA_GART_SIZE - 1,
	}
};

struct platform_device tegra_gart_device = {
	.name		= "tegra_gart",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(tegra_gart_resources),
	.resource	= tegra_gart_resources
};
#endif

#if defined(CONFIG_TEGRA_IOVMM_SMMU)
static struct resource tegra_smmu_resources[] = {
	[0] = {
		.name	= "mc",
		.flags	= IORESOURCE_MEM,
		.start	= TEGRA_MC_BASE,
		.end	= TEGRA_MC_BASE + TEGRA_MC_SIZE - 1,
	},
	[1] = {
		.name	= "smmu",
		.flags	= IORESOURCE_MEM,
		.start	= TEGRA_SMMU_BASE,
		.end	= TEGRA_SMMU_BASE + TEGRA_SMMU_SIZE - 1,
	}
};

struct platform_device tegra_smmu_device = {
	.name		= "tegra_smmu",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(tegra_smmu_resources),
	.resource	= tegra_smmu_resources
};
#endif

static struct resource pmu_resources[] = {
	[0] = {
		.start	= INT_CPU0_PMU_INTR,
		.end	= INT_CPU0_PMU_INTR,
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= INT_CPU1_PMU_INTR,
		.end	= INT_CPU1_PMU_INTR,
		.flags	= IORESOURCE_IRQ,
	},
#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
	[2] = {
		.start	= INT_CPU2_PMU_INTR,
		.end	= INT_CPU2_PMU_INTR,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.start	= INT_CPU3_PMU_INTR,
		.end	= INT_CPU3_PMU_INTR,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

struct platform_device pmu_device = {
	.name		= "arm-pmu",
	.id		= ARM_PMU_DEVICE_CPU,
	.num_resources	= ARRAY_SIZE(pmu_resources),
	.resource	= pmu_resources,
};

static struct resource tegra_wdt_resources[] = {
	[0] = {
		.start	= TEGRA_WDT0_BASE,
		.end	= TEGRA_WDT0_BASE + TEGRA_WDT0_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= TEGRA_TMR6_BASE,
		.end	= TEGRA_TMR6_BASE + TEGRA_TMR6_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= INT_WDT_CPU,
		.end	= INT_WDT_CPU,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device tegra_wdt_device = {
	.name		= "tegra_wdt",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(tegra_wdt_resources),
	.resource	= tegra_wdt_resources,
};

static struct resource tegra_pwfm0_resource = {
	.start	= TEGRA_PWFM0_BASE,
	.end	= TEGRA_PWFM0_BASE + TEGRA_PWFM0_SIZE - 1,
	.flags	= IORESOURCE_MEM,
};

static struct resource tegra_pwfm1_resource = {
	.start	= TEGRA_PWFM1_BASE,
	.end	= TEGRA_PWFM1_BASE + TEGRA_PWFM1_SIZE - 1,
	.flags	= IORESOURCE_MEM,
};

static struct resource tegra_pwfm2_resource = {
	.start	= TEGRA_PWFM2_BASE,
	.end	= TEGRA_PWFM2_BASE + TEGRA_PWFM2_SIZE - 1,
	.flags	= IORESOURCE_MEM,
};

static struct resource tegra_pwfm3_resource = {
	.start	= TEGRA_PWFM3_BASE,
	.end	= TEGRA_PWFM3_BASE + TEGRA_PWFM3_SIZE - 1,
	.flags	= IORESOURCE_MEM,
};

struct platform_device tegra_pwfm0_device = {
	.name		= "tegra_pwm",
	.id		= 0,
	.num_resources	= 1,
	.resource	= &tegra_pwfm0_resource,
};

struct platform_device tegra_pwfm1_device = {
	.name		= "tegra_pwm",
	.id		= 1,
	.num_resources	= 1,
	.resource	= &tegra_pwfm1_resource,
};

struct platform_device tegra_pwfm2_device = {
	.name		= "tegra_pwm",
	.id		= 2,
	.num_resources	= 1,
	.resource	= &tegra_pwfm2_resource,
};

struct platform_device tegra_pwfm3_device = {
	.name		= "tegra_pwm",
	.id		= 3,
	.num_resources	= 1,
	.resource	= &tegra_pwfm3_resource,
};

static struct resource tegra_uarta_resources[] = {
	[0] = {
		.start 	= TEGRA_UARTA_BASE,
		.end	= TEGRA_UARTA_BASE + TEGRA_UARTA_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_UARTA,
		.end	= INT_UARTA,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource tegra_uartb_resources[]= {
	[0] = {
		.start 	= TEGRA_UARTB_BASE,
		.end	= TEGRA_UARTB_BASE + TEGRA_UARTB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_UARTB,
		.end	= INT_UARTB,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource tegra_uartc_resources[] = {
	[0] = {
		.start 	= TEGRA_UARTC_BASE,
		.end	= TEGRA_UARTC_BASE + TEGRA_UARTC_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_UARTC,
		.end	= INT_UARTC,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource tegra_uartd_resources[] = {
	[0] = {
		.start 	= TEGRA_UARTD_BASE,
		.end	= TEGRA_UARTD_BASE + TEGRA_UARTD_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_UARTD,
		.end	= INT_UARTD,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource tegra_uarte_resources[] = {
	[0] = {
		.start 	= TEGRA_UARTE_BASE,
		.end	= TEGRA_UARTE_BASE + TEGRA_UARTE_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_UARTE,
		.end	= INT_UARTE,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device tegra_uarta_device = {
	.name	= "tegra_uart",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(tegra_uarta_resources),
	.resource	= tegra_uarta_resources,
	.dev	= {
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

struct platform_device tegra_uartb_device = {
	.name	= "tegra_uart",
	.id	= 1,
	.num_resources	= ARRAY_SIZE(tegra_uartb_resources),
	.resource	= tegra_uartb_resources,
	.dev	= {
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

struct platform_device tegra_uartc_device = {
	.name	= "tegra_uart",
	.id	= 2,
	.num_resources	= ARRAY_SIZE(tegra_uartc_resources),
	.resource	= tegra_uartc_resources,
	.dev	= {
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

struct platform_device tegra_uartd_device = {
	.name	= "tegra_uart",
	.id	= 3,
	.num_resources	= ARRAY_SIZE(tegra_uartd_resources),
	.resource	= tegra_uartd_resources,
	.dev	= {
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

struct platform_device tegra_uarte_device = {
	.name	= "tegra_uart",
	.id	= 4,
	.num_resources	= ARRAY_SIZE(tegra_uarte_resources),
	.resource	= tegra_uarte_resources,
	.dev	= {
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

static struct resource tegra_grhost_resources[] = {
	{
		.start = TEGRA_HOST1X_BASE,
		.end = TEGRA_HOST1X_BASE + TEGRA_HOST1X_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = TEGRA_DISPLAY_BASE,
		.end = TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = TEGRA_DISPLAY2_BASE,
		.end = TEGRA_DISPLAY2_BASE + TEGRA_DISPLAY2_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = TEGRA_VI_BASE,
		.end = TEGRA_VI_BASE + TEGRA_VI_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = TEGRA_ISP_BASE,
		.end = TEGRA_ISP_BASE + TEGRA_ISP_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = TEGRA_MPE_BASE,
		.end = TEGRA_MPE_BASE + TEGRA_MPE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = INT_SYNCPT_THRESH_BASE,
		.end = INT_SYNCPT_THRESH_BASE + INT_SYNCPT_THRESH_NR - 1,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = INT_HOST1X_MPCORE_GENERAL,
		.end = INT_HOST1X_MPCORE_GENERAL,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device tegra_grhost_device = {
	.name = "tegra_grhost",
	.id = -1,
	.resource = tegra_grhost_resources,
	.num_resources = ARRAY_SIZE(tegra_grhost_resources),
};

static struct resource tegra_avp_resources[] = {
	[0] = {
		.start	= INT_SHR_SEM_INBOX_IBF,
		.end	= INT_SHR_SEM_INBOX_IBF,
		.flags	= IORESOURCE_IRQ,
		.name	= "mbox_from_avp_pending",
	},
};

struct platform_device tegra_avp_device = {
	.name		= "tegra-avp",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(tegra_avp_resources),
	.resource	= tegra_avp_resources,
	.dev  = {
		.coherent_dma_mask	= 0xffffffffULL,
	},
};

static struct resource tegra_aes_resources[] = {
	{
		.start	= TEGRA_VDE_BASE,
		.end	= TEGRA_VDE_BASE + TEGRA_VDE_BASE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= TEGRA_BSEA_BASE,
		.end	= TEGRA_BSEA_BASE + TEGRA_BSEA_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static u64 tegra_aes_dma_mask = DMA_BIT_MASK(32);

struct platform_device tegra_aes_device = {
	.name		= "tegra-aes",
	.id		= -1,
	.resource	= tegra_aes_resources,
	.num_resources	= ARRAY_SIZE(tegra_aes_resources),
	.dev	= {
		.dma_mask = &tegra_aes_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};
