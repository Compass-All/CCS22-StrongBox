/*
 *
 * (C) COPYRIGHT 2017 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/scmi_protocol.h>
#include <linux/version.h>

static const struct scmi_handle *handle;
static int gpu_domain_id;
static struct platform_device *scmi_device;
struct platform_device *scmi_pdev;

struct scmi_handle *scmi_handle_get(struct device *dev);

static int scmi_gpu_opps_probe(struct platform_device *pdev)
{
	struct device_node *np ;
	struct platform_device *gpu_pdev;
	struct device *dev = &pdev->dev;
	int err;

	handle = scmi_handle_get(dev);
	if (IS_ERR_OR_NULL(handle) || !handle->perf_ops){
		return -EPROBE_DEFER;
	}

	np = of_find_node_by_name(NULL, "gpu");
	if (!np) {
		dev_err(dev, "Failed to find DT entry for Mali\n");
		return -EFAULT;
	}

	gpu_pdev = of_find_device_by_node(np);
	if (!gpu_pdev) {
		dev_err(dev, "Failed to find device for Mali\n");
		of_node_put(np);
		return -EFAULT;
	}

	gpu_domain_id = handle->perf_ops->device_domain_id(&gpu_pdev->dev);

	err = handle->perf_ops->add_opps_to_device(handle, &gpu_pdev->dev);

	of_node_put(np);

	return err;

}

int scmi_gpu_domain_id_get(void)
{
	return gpu_domain_id;
}

const struct scmi_handle *scmi_gpu_handle_get(void)
{
	if (IS_ERR_OR_NULL(handle) || !handle->perf_ops)
		return ERR_PTR(-EPROBE_DEFER);
	return handle;
}

static struct platform_driver scmi_gpu_opp_driver = {
	.driver = {
		.name = "scmi_gpu_opp",
	},
	.probe = scmi_gpu_opps_probe,
};

static int __init scmi_gpu_opp_init (void)
{
	int ret;
	struct device_node *np_scmi;
	np_scmi = of_find_node_by_name(NULL, "scmi");
	if (!np_scmi) {
                printk("Failed to find DT entry for scmi\n");
                return -EPROBE_DEFER;
        }

	scmi_pdev = of_find_device_by_node(np_scmi);
	if (!scmi_pdev) {
                printk( "Failed to find device for scmi\n");
                of_node_put(np_scmi);
                return -EFAULT;
	}

	ret = platform_driver_register(&scmi_gpu_opp_driver);
	if (ret)
		goto exit;

	scmi_device = platform_device_register_resndata(&scmi_pdev->dev,
							"scmi_gpu_opp",
							-1, NULL, 0, NULL, 0);
	if (IS_ERR(scmi_device)) {
		platform_driver_unregister(&scmi_gpu_opp_driver);
		return -ENODEV;
	}

exit:
	return ret;
}

static void __exit scmi_gpu_opp_exit (void)
{
	platform_device_unregister(scmi_device);
	platform_driver_unregister(&scmi_gpu_opp_driver);
}

module_init(scmi_gpu_opp_init);
module_exit(scmi_gpu_opp_exit);

MODULE_AUTHOR("Amit Kachhap <amit.kachhap@arm.com>");
MODULE_DESCRIPTION("ARM SCMI GPU opp driver");
MODULE_LICENSE("GPL v2");
