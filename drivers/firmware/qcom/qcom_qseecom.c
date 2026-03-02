// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Driver for Qualcomm Secure Execution Environment (SEE) interface (QSEECOM).
 * Responsible for setting up and managing QSEECOM client devices.
 *
 * Copyright (C) 2023 Maximilian Luz <luzmaximilian@gmail.com>
 */
#include <linux/auxiliary_bus.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>

#include <linux/firmware/qcom/qcom_qseecom.h>
#include <linux/firmware/qcom/qcom_scm.h>

struct qseecom_app_desc {
	const char *app_name;	/* TZ app name (for SCM get_id and load request) */
	const char *dev_name;
	const char *fw_name;	/* firmware file prefix, if different from app_name */
	bool legacy_fallback;	/* try legacy QSEOS protocol if modern returns -EOPNOTSUPP */
};

static void qseecom_client_release(struct device *dev)
{
	struct qseecom_client *client;

	client = container_of(dev, struct qseecom_client, aux_dev.dev);
	kfree(client);
}

static void qseecom_client_remove(void *data)
{
	struct qseecom_client *client = data;

	auxiliary_device_delete(&client->aux_dev);
	auxiliary_device_uninit(&client->aux_dev);
}

static int qseecom_client_register(struct platform_device *qseecom_dev,
				   const struct qseecom_app_desc *desc)
{
	struct qseecom_client *client;
	u32 app_id;
	int ret;

	bool legacy = false;
	
	ret = qcom_scm_qseecom_app_get_id(desc->app_name, &app_id);
	if (ret == -EOPNOTSUPP && desc->legacy_fallback) {
		ret = qcom_scm_qseecom_legacy_app_get_id(desc->app_name, &app_id);
		if (ret == -ENOENT) {
			ret = qcom_scm_qseecom_legacy_app_load(desc->app_name,
							    desc->fw_name ?: desc->app_name,
							    &app_id);
			if (ret == -ENOENT || ret == -EIO) {
				dev_dbg(&qseecom_dev->dev,
					"firmware for %s not available yet, deferring probe\n",
					desc->app_name);
				return -EPROBE_DEFER;
			}
		}
		if (ret == 0)
			legacy = true;
	}
	if (ret)
		return (ret == -ENOENT || ret == -EOPNOTSUPP) ? 0 : ret;

	dev_info(&qseecom_dev->dev, "setting up client for %s (%s protocol)\n",
		 desc->app_name, legacy ? "legacy QSEOS" : "modern SMCCC");

	/* Allocate and set-up the client device */
	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	client->aux_dev.name = desc->dev_name;
	client->aux_dev.dev.parent = &qseecom_dev->dev;
	client->aux_dev.dev.release = qseecom_client_release;
	client->app_id = app_id;
	client->legacy_protocol = legacy;

	ret = auxiliary_device_init(&client->aux_dev);
	if (ret) {
		kfree(client);
		return ret;
	}

	ret = auxiliary_device_add(&client->aux_dev);
	if (ret) {
		auxiliary_device_uninit(&client->aux_dev);
		return ret;
	}

	ret = devm_add_action_or_reset(&qseecom_dev->dev, qseecom_client_remove, client);
	if (ret)
		return ret;

	return 0;
}

/*
 * List of supported applications. One client device will be created per entry,
 * assuming the app has already been loaded (usually by firmware bootloaders)
 * and its ID can be queried successfully.
 */
static const struct qseecom_app_desc qcom_qseecom_apps[] = {
	{ "qcom.tz.uefisecapp", "uefisecapp" },
	{ "keymaster", "keymaster", .fw_name = "keymaste", .legacy_fallback = true },
};

static int qcom_qseecom_probe(struct platform_device *qseecom_dev)
{
	int ret;
	int i;

	/* Set up client devices for each base application */
	for (i = 0; i < ARRAY_SIZE(qcom_qseecom_apps); i++) {
		ret = qseecom_client_register(qseecom_dev, &qcom_qseecom_apps[i]);
		if (ret)
			return ret;
	}

	return 0;
}

static struct platform_driver qcom_qseecom_driver = {
	.driver = {
		.name	= "qcom_qseecom",
	},
	.probe = qcom_qseecom_probe,
};

static int __init qcom_qseecom_init(void)
{
	return platform_driver_register(&qcom_qseecom_driver);
}

device_initcall(qcom_qseecom_init);

MODULE_AUTHOR("Maximilian Luz <luzmaximilian@gmail.com>");
MODULE_DESCRIPTION("Driver for the Qualcomm SEE (QSEECOM) interface");
MODULE_LICENSE("GPL");
