// SPDX-License-Identifier: GPL-2.0
/*
 * mt79xx-wm8960.c  --  MT79xx WM8960 ALSA SoC machine driver
 *
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Vic Wu <vic.wu@mediatek.com>
 */

#include <linux/module.h>
#include <sound/soc.h>

#include "mt79xx-afe-common.h"

struct mt79xx_wm8960_priv {
	struct device_node *platform_node;
	struct device_node *codec_node;
};

static const struct snd_soc_dapm_widget mt79xx_wm8960_widgets[] = {
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_MIC("AMIC", NULL),
};

static const struct snd_kcontrol_new mt79xx_wm8960_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphone"),
	SOC_DAPM_PIN_SWITCH("AMIC"),
};

SND_SOC_DAILINK_DEFS(playback,
	DAILINK_COMP_ARRAY(COMP_CPU("DL1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(capture,
	DAILINK_COMP_ARRAY(COMP_CPU("UL1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(codec,
	DAILINK_COMP_ARRAY(COMP_CPU("ETDM")),
	DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "wm8960-hifi")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link mt79xx_wm8960_dai_links[] = {
	/* FE */
	{
		.name = "wm8960-playback",
		.stream_name = "wm8960-playback",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback),
	},
	{
		.name = "wm8960-capture",
		.stream_name = "wm8960-capture",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture),
	},
	/* BE */
	{
		.name = "wm8960-codec",
		.no_pcm = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS |
			SND_SOC_DAIFMT_GATED,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(codec),
	},
};

static struct snd_soc_card mt79xx_wm8960_card = {
	.name = "mt79xx-wm8960",
	.owner = THIS_MODULE,
	.dai_link = mt79xx_wm8960_dai_links,
	.num_links = ARRAY_SIZE(mt79xx_wm8960_dai_links),
	.controls = mt79xx_wm8960_controls,
	.num_controls = ARRAY_SIZE(mt79xx_wm8960_controls),
	.dapm_widgets = mt79xx_wm8960_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt79xx_wm8960_widgets),
};

static int mt79xx_wm8960_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt79xx_wm8960_card;
	struct snd_soc_dai_link *dai_link;
	struct mt79xx_wm8960_priv *priv;
	int ret, i;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->platform_node = of_parse_phandle(pdev->dev.of_node,
					       "mediatek,platform", 0);
	if (!priv->platform_node) {
		dev_err(&pdev->dev, "Property 'platform' missing or invalid\n");
		return -EINVAL;
	}

	for_each_card_prelinks(card, i, dai_link) {
		if (dai_link->platforms->name)
			continue;
		dai_link->platforms->of_node = priv->platform_node;
	}

	card->dev = &pdev->dev;

	priv->codec_node = of_parse_phandle(pdev->dev.of_node,
					    "mediatek,audio-codec", 0);
	if (!priv->codec_node) {
		dev_err(&pdev->dev,
			"Property 'audio-codec' missing or invalid\n");
		of_node_put(priv->platform_node);
		return -EINVAL;
	}

	for_each_card_prelinks(card, i, dai_link) {
		if (dai_link->codecs->name)
			continue;
		dai_link->codecs->of_node = priv->codec_node;
	}

	ret = snd_soc_of_parse_audio_routing(card, "audio-routing");
	if (ret) {
		dev_err(&pdev->dev, "failed to parse audio-routing: %d\n", ret);
		goto err_of_node_put;
	}

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret) {
		dev_err(&pdev->dev, "%s snd_soc_register_card fail %d\n",
			__func__, ret);
		goto err_of_node_put;
	}

err_of_node_put:
	of_node_put(priv->codec_node);
	of_node_put(priv->platform_node);
	return ret;
}

static int mt79xx_wm8960_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct mt79xx_wm8960_priv *priv = snd_soc_card_get_drvdata(card);

	of_node_put(priv->codec_node);
	of_node_put(priv->platform_node);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt79xx_wm8960_machine_dt_match[] = {
	{.compatible = "mediatek,mt79xx-wm8960-machine",},
	{}
};
#endif

static struct platform_driver mt79xx_wm8960_machine = {
	.driver = {
		.name = "mt79xx-wm8960",
#ifdef CONFIG_OF
		.of_match_table = mt79xx_wm8960_machine_dt_match,
#endif
	},
	.probe = mt79xx_wm8960_machine_probe,
	.remove = mt79xx_wm8960_machine_remove,
};

module_platform_driver(mt79xx_wm8960_machine);

/* Module information */
MODULE_DESCRIPTION("MT79xx WM8960 ALSA SoC machine driver");
MODULE_AUTHOR("Vic Wu <vic.wu@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("mt79xx wm8960 soc card");
