// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for ROCKCHIP Integrated FEPHYs
 *
 * Copyright (c) 2025, Rockchip Electronics Co., Ltd.
 *
 * David Wu <david.wu@rock-chips.com>
 */

#include <linux/ethtool.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/netdevice.h>
#include <linux/of_irq.h>
#include <linux/phy.h>

#define INTERNAL_FEPHY_ID			0x06808101

#define MII_INTERNAL_CTRL_STATUS		17
#define SMI_ADDR_TSTCNTL			20
#define SMI_ADDR_TSTREAD1			21
#define SMI_ADDR_TSTREAD2			22
#define SMI_ADDR_TSTWRITE			23
#define MII_LED_CTRL				25
#define MII_INT_STATUS				29
#define MII_INT_MASK				30
#define MII_SPECIAL_CONTROL_STATUS		31

#define MII_AUTO_MDIX_EN			BIT(7)
#define MII_MDIX_EN				BIT(6)

#define MII_SPEED_10				BIT(2)
#define MII_SPEED_100				BIT(3)

#define TSTCNTL_WRITE_ADDR			0
#define TSTCNTL_READ_ADDR			5
#define TSTCNTL_BANK_SEL			11
#define TSTCNTL_RD				(BIT(15) | BIT(10))
#define TSTCNTL_WR				(BIT(14) | BIT(10))

#define TSTCNTL_WRITE(bank, reg)		(TSTCNTL_WR | ((bank) << TSTCNTL_BANK_SEL) \
						| ((reg) << TSTCNTL_WRITE_ADDR))
#define TSTCNTL_READ(bank, reg)			(TSTCNTL_RD | ((bank) << TSTCNTL_BANK_SEL) \
						| ((reg) << TSTCNTL_READ_ADDR))

#define TSTMODE_ENABLE				0x400
#define TSTMODE_DISABLE				0x0

#define WR_ADDR_A7CFG				0x18

enum {
	BANK_DSP0 = 0,
	BANK_WOL,
	BANK_BIST = 3,
	BANK_AFE,
	BANK_DSP1
};

struct rockchip_fephy_priv {
	struct phy_device *phydev;
	int wol_irq;
};

static int rockchip_fephy_init_tstmode(struct phy_device *phydev)
{
	int ret;

	ret = phy_write(phydev, SMI_ADDR_TSTCNTL, TSTMODE_DISABLE);
	if (ret)
		return ret;

	ret = phy_write(phydev, SMI_ADDR_TSTCNTL, TSTMODE_ENABLE);
	if (ret)
		return ret;

	ret = phy_write(phydev, SMI_ADDR_TSTCNTL, TSTMODE_DISABLE);
	if (ret)
		return ret;

	return phy_write(phydev, SMI_ADDR_TSTCNTL, TSTMODE_ENABLE);
}

static int rockchip_fephy_close_tstmode(struct phy_device *phydev)
{
	/* Back to basic register bank */
	return phy_write(phydev, SMI_ADDR_TSTCNTL, TSTMODE_DISABLE);
}

static int rockchip_fephy_bank_write(struct phy_device *phydev, u8 bank,
				     u32 reg, u16 val)
{
	int ret;

	ret = phy_write(phydev, SMI_ADDR_TSTWRITE, val);
	if (ret)
		return ret;

	return phy_write(phydev, SMI_ADDR_TSTCNTL, TSTCNTL_WRITE(bank, reg));
}

static int rockchip_fephy_config_init(struct phy_device *phydev)
{
	int ret;

	/* LED Control, default:0x7f */
	ret = phy_write(phydev, MII_LED_CTRL, 0x7aa);
	if (ret)
		return ret;

	ret = rockchip_fephy_init_tstmode(phydev);
	if (ret)
		return ret;

	/* 100M amplitude control */
	ret = rockchip_fephy_bank_write(phydev, BANK_DSP0, 0x18, 0xc);
	if (ret)
		return ret;

	ret = rockchip_fephy_close_tstmode(phydev);
	if (ret)
		return ret;

	return ret;
}

static int rockchip_fephy_config_aneg(struct phy_device *phydev)
{
	return genphy_config_aneg(phydev);
}

static int rockchip_fephy_wol_enable(struct phy_device *phydev)
{
	struct net_device *ndev = phydev->attached_dev;
	int ret;

	ret = rockchip_fephy_bank_write(phydev, BANK_WOL, 0x0,
					((u16)ndev->dev_addr[4] << 8) + ndev->dev_addr[5]);
	if (ret)
		return ret;

	ret = rockchip_fephy_bank_write(phydev, BANK_WOL, 0x1,
					((u16)ndev->dev_addr[2] << 8) + ndev->dev_addr[3]);
	if (ret)
		return ret;

	ret = rockchip_fephy_bank_write(phydev, BANK_WOL, 0x2,
					((u16)ndev->dev_addr[0] << 8) + ndev->dev_addr[1]);
	if (ret)
		return ret;

	ret = rockchip_fephy_bank_write(phydev, BANK_WOL, 0x3, 0xf);
	if (ret)
		return ret;

	/* Enable WOL interrupt */
	ret = phy_write(phydev, MII_INT_MASK, 0xe00);
	if (ret)
		return ret;

	return ret;
}

static int rockchip_fephy_wol_disable(struct phy_device *phydev)
{
	int ret;

	ret = rockchip_fephy_bank_write(phydev, BANK_WOL, 0x3, 0x0);
	if (ret)
		return ret;

	/* Disable WOL interrupt */
	ret = phy_write(phydev, MII_INT_MASK, 0x0);
	if (ret)
		return ret;

	return ret;
}

static irqreturn_t rockchip_fephy_wol_irq_thread(int irq, void *dev_id)
{
	struct rockchip_fephy_priv *priv = (struct rockchip_fephy_priv *)dev_id;

	/* Read status to ack interrupt */
	phy_read(priv->phydev, MII_INT_STATUS);

	return IRQ_HANDLED;
}

static int rockchip_fephy_probe(struct phy_device *phydev)
{
	struct rockchip_fephy_priv *priv;
	int ret;

	priv = devm_kzalloc(&phydev->mdio.dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	phydev->priv = priv;

	priv->wol_irq = of_irq_get_byname(phydev->mdio.dev.of_node, "wol_irq");
	if (priv->wol_irq == -EPROBE_DEFER)
		return priv->wol_irq;

	if (priv->wol_irq > 0) {
		ret = devm_request_threaded_irq(&phydev->mdio.dev, priv->wol_irq,
						NULL, rockchip_fephy_wol_irq_thread,
						IRQF_TRIGGER_RISING | IRQF_ONESHOT,
						"rockchip_fephy_wol_irq", priv);
		if (ret) {
			phydev_err(phydev, "request wol_irq failed: %d\n", ret);
			goto irq_err;
		}
		disable_irq(priv->wol_irq);
		enable_irq_wake(priv->wol_irq);
	}

	priv->phydev = phydev;
	return 0;

irq_err:

	return ret;
}

static void rockchip_fephy_remove(struct phy_device *phydev)
{
}

static int rockchip_fephy_suspend(struct phy_device *phydev)
{
	struct rockchip_fephy_priv *priv = phydev->priv;

	if (priv->wol_irq > 0) {
		rockchip_fephy_wol_enable(phydev);
		enable_irq(priv->wol_irq);
	}

	return genphy_suspend(phydev);
}

static int rockchip_fephy_resume(struct phy_device *phydev)
{
	struct rockchip_fephy_priv *priv = phydev->priv;

	if (priv->wol_irq > 0) {
		rockchip_fephy_wol_disable(phydev);
		disable_irq(priv->wol_irq);
	}

	return genphy_resume(phydev);
}

static struct phy_driver rockchip_fephy_driver[] = {
{
	.phy_id			= INTERNAL_FEPHY_ID,
	.phy_id_mask		= 0xffffffff,
	.name			= "Rockchip integrated FEPHY",
	/* PHY_BASIC_FEATURES */
	.features		= PHY_BASIC_FEATURES,
	.flags			= 0,
	.soft_reset		= genphy_soft_reset,
	.config_init		= rockchip_fephy_config_init,
	.config_aneg		= rockchip_fephy_config_aneg,
	.probe			= rockchip_fephy_probe,
	.remove			= rockchip_fephy_remove,
	.suspend		= rockchip_fephy_suspend,
	.resume			= rockchip_fephy_resume,
},
};

module_phy_driver(rockchip_fephy_driver);

static struct mdio_device_id __maybe_unused rockchip_fephy_tbl[] = {
	{ INTERNAL_FEPHY_ID, 0xffffffff },
	{ }
};

MODULE_DEVICE_TABLE(mdio, rockchip_fephy_tbl);

MODULE_AUTHOR("David Wu <david.wu@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip integrated FEPHYs driver");
MODULE_LICENSE("GPL");
